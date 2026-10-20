#include "main_window.h"

#include "auts_io.h"
#include "default_palette.h"
#include "export_settings.h"
#include "game_profile.h"
#include "lev_reader.h"
#include "lev_writer.h"
#include "layer_model.h"
#include "level_canvas.h"
#include "overthrust_profile.h"
#include "canonical_png.h"
#include "overthrust_package.h"
#include "palette_io.h"
#include "palette_groups.h"
#include "palette_rules.h"
#include "palette_widget.h"
#include "pcx_reader.h"
#include "pcx_writer.h"
#include "pcxl_project_io.h"
#include "wings_lev_writer.h"

#include <QAction>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QColorSpace>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QCryptographicHash>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QInputDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPolygon>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QVBoxLayout>
#include <QValidator>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class PaletteIndexSpinBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

    void setGame(const GameId game)
    {
        game_ = game;
        if (isReservedColorChartNumber(game_, value())) {
            setValue(game_ == GameId::Wings ? 128
                                            : game_ == GameId::Auts ? 7 : 56);
        }
    }

protected:
    void stepBy(const int steps) override
    {
        const int direction = steps < 0 ? -1 : 1;
        int candidate = value();
        for (int step = 0; step < std::abs(steps); ++step) {
            do {
                candidate += direction;
            } while (candidate >= minimum() && candidate <= maximum() &&
                     isReservedColorChartNumber(game_, candidate));
            candidate = std::clamp(candidate, minimum(), maximum());
        }
        setValue(candidate);
    }

    QValidator::State validate(QString& input, int& position) const override
    {
        const QValidator::State state = QSpinBox::validate(input, position);
        if (state == QValidator::Acceptable &&
            isReservedColorChartNumber(game_, input.toInt())) {
            return QValidator::Intermediate;
        }
        return state;
    }

    void fixup(QString& input) const override
    {
        bool valid = false;
        int value = input.toInt(&valid);
        if (valid && isReservedColorChartNumber(game_, value)) {
            while (value <= maximum() &&
                   isReservedColorChartNumber(game_, value)) {
                ++value;
            }
            input = QString::number(std::min(value, maximum()));
            return;
        }
        QSpinBox::fixup(input);
    }

private:
    GameId game_ = GameId::VWing;
};

namespace {

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

QString toQString(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

bool dimensionsFitProfile(const GameProfile& profile, const std::size_t width,
                          const std::size_t height)
{
    return width >= static_cast<std::size_t>(profile.minimumWidth) &&
           width <= static_cast<std::size_t>(profile.maximumWidth) &&
           height >= static_cast<std::size_t>(profile.minimumHeight) &&
           height <= static_cast<std::size_t>(profile.maximumHeight) &&
           height != 0 && width <= profile.maximumCells / height;
}

bool validateBackgroundContainer(const QString& filename,
                                 const QByteArray& format, QString& error)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    const auto be32 = [&](const qsizetype offset) -> quint32 {
        return static_cast<quint32>(
            static_cast<unsigned char>(bytes[offset])) << 24 |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 1])) << 16 |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 2])) << 8 |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 3]));
    };
    const auto le32 = [&](const qsizetype offset) -> quint32 {
        return static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset])) |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 1])) << 8 |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 2])) << 16 |
               static_cast<quint32>(
                   static_cast<unsigned char>(bytes[offset + 3])) << 24;
    };
    if (format == "png") {
        const QByteArray signature("\x89PNG\r\n\x1a\n", 8);
        if (bytes.size() < 33 || !bytes.startsWith(signature) ||
            be32(8) != 13 || bytes.mid(12, 4) != "IHDR" ||
            static_cast<unsigned char>(bytes[24]) > 8) {
            error = QObject::tr("PNG header is malformed or uses more than 8 bits per channel.");
            return false;
        }
        qsizetype offset = 8;
        bool ended = false;
        while (offset + 12 <= bytes.size()) {
            const quint32 size = be32(offset);
            if (size > static_cast<quint32>(bytes.size() - offset - 12)) {
                error = QObject::tr("PNG contains a truncated chunk.");
                return false;
            }
            const QByteArray type = bytes.mid(offset + 4, 4);
            if (type == "acTL") {
                error = QObject::tr("Animated PNG backgrounds are not supported.");
                return false;
            }
            offset += 12 + size;
            if (type == "IEND") {
                ended = true;
                break;
            }
        }
        if (!ended || offset != bytes.size()) {
            error = QObject::tr("PNG has trailing or malformed data.");
            return false;
        }
    } else if (format == "jpeg" || format == "jpg") {
        if (bytes.size() < 4 ||
            static_cast<unsigned char>(bytes[0]) != 0xff ||
            static_cast<unsigned char>(bytes[1]) != 0xd8 ||
            static_cast<unsigned char>(bytes[bytes.size() - 2]) != 0xff ||
            static_cast<unsigned char>(bytes[bytes.size() - 1]) != 0xd9) {
            error = QObject::tr("JPEG has a malformed signature or trailing data.");
            return false;
        }
        qsizetype offset = 2;
        while (offset + 4 <= bytes.size()) {
            while (offset < bytes.size() &&
                   static_cast<unsigned char>(bytes[offset]) == 0xff) ++offset;
            if (offset >= bytes.size()) break;
            const unsigned char marker =
                static_cast<unsigned char>(bytes[offset++]);
            if (marker == 0xda || marker == 0xd9) break;
            if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;
            if (offset + 2 > bytes.size()) break;
            const quint16 size = static_cast<quint16>(
                static_cast<unsigned char>(bytes[offset]) << 8 |
                static_cast<unsigned char>(bytes[offset + 1]));
            if (size < 2 || offset + size > bytes.size()) {
                error = QObject::tr("JPEG contains a malformed segment.");
                return false;
            }
            const bool startOfFrame =
                (marker >= 0xc0 && marker <= 0xc3) ||
                (marker >= 0xc5 && marker <= 0xc7) ||
                (marker >= 0xc9 && marker <= 0xcb) ||
                (marker >= 0xcd && marker <= 0xcf);
            if (startOfFrame &&
                (size < 8 || static_cast<unsigned char>(bytes[offset + 2]) > 8 ||
                 static_cast<unsigned char>(bytes[offset + 7]) == 4)) {
                error = QObject::tr("JPEG must be 8-bit RGB, not CMYK or high-bit-depth.");
                return false;
            }
            offset += size;
        }
    } else if (format == "webp") {
        if (bytes.size() < 20 || bytes.mid(0, 4) != "RIFF" ||
            bytes.mid(8, 4) != "WEBP" || le32(4) + 8ULL !=
                                               static_cast<quint64>(bytes.size())) {
            error = QObject::tr("WebP container is malformed or has trailing data.");
            return false;
        }
        qsizetype offset = 12;
        while (offset + 8 <= bytes.size()) {
            const QByteArray type = bytes.mid(offset, 4);
            const quint32 size = le32(offset + 4);
            if (size > static_cast<quint32>(bytes.size() - offset - 8)) {
                error = QObject::tr("WebP contains a truncated chunk.");
                return false;
            }
            if (type == "ANIM" || type == "ANMF" ||
                (type == "VP8X" && size >= 1 &&
                 (static_cast<unsigned char>(bytes[offset + 8]) & 0x02))) {
                error = QObject::tr("Animated WebP backgrounds are not supported.");
                return false;
            }
            offset += 8 + size + (size & 1U);
        }
        if (offset != bytes.size()) {
            error = QObject::tr("WebP contains malformed trailing bytes.");
            return false;
        }
    }
    return true;
}

bool loadOverThrustBackground(const QString& filename,
                              const std::size_t materialWidth,
                              const std::size_t materialHeight,
                              OverThrustBackground& background,
                              QString& error)
{
    const QFileInfo fileInfo(filename);
    if (!fileInfo.exists() || fileInfo.size() < 1) {
        error = QObject::tr("Background source is missing or empty.");
        return false;
    }
    QImageReader reader(filename);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    const QByteArray format = reader.format().toLower();
    const QString extension = fileInfo.suffix().toLower();
    const bool formatAllowed = format == "png" || format == "jpeg" ||
                               format == "jpg" || format == "webp";
    const bool extensionMatches =
        (format == "png" && extension == QStringLiteral("png")) ||
        ((format == "jpeg" || format == "jpg") &&
         (extension == QStringLiteral("jpg") ||
          extension == QStringLiteral("jpeg"))) ||
        (format == "webp" && extension == QStringLiteral("webp"));
    if (!formatAllowed || !extensionMatches) {
        error = QObject::tr(
            "The background signature must match a PNG, JPEG, or WebP extension.");
        return false;
    }
    if (!validateBackgroundContainer(filename, format, error)) {
        return false;
    }
    if (reader.supportsAnimation() || reader.imageCount() > 1) {
        error = QObject::tr("Animated or multi-image backgrounds are not supported.");
        return false;
    }
    const QSize decodedSize = reader.size();
    if (!decodedSize.isValid() ||
        !overthrustBackgroundDimensionsAllowed(
            static_cast<std::size_t>(decodedSize.width()),
            static_cast<std::size_t>(decodedSize.height()), materialWidth,
            materialHeight)) {
        error = QObject::tr(
            "Background pixel dimensions are outside the allowed range for this world size.");
        return false;
    }
    QImage image = reader.read();
    if (image.isNull()) {
        error = reader.errorString();
        return false;
    }
    if (image.format() == QImage::Format_RGBA64 ||
        image.format() == QImage::Format_RGBA64_Premultiplied ||
        image.format() == QImage::Format_RGBX64 ||
        image.format() == QImage::Format_Grayscale16) {
        error = QObject::tr("Backgrounds must use RGB/RGBA with at most 8 bits per channel.");
        return false;
    }
    if (image.colorSpace().isValid() &&
        image.colorSpace() != QColorSpace(QColorSpace::SRgb)) {
        image.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    if (image.isNull()) {
        error = QObject::tr("Could not normalize the background to sRGB RGBA8.");
        return false;
    }
    background.width = static_cast<std::uint32_t>(image.width());
    background.height = static_cast<std::uint32_t>(image.height());
    background.rgba.resize(static_cast<std::size_t>(image.width()) *
                           image.height() * 4);
    const std::size_t rowBytes = static_cast<std::size_t>(image.width()) * 4;
    for (int y = 0; y < image.height(); ++y) {
        std::copy_n(image.constScanLine(y), rowBytes,
                    background.rgba.begin() +
                        static_cast<std::ptrdiff_t>(y * rowBytes));
    }
    QCryptographicHash hashBuilder(QCryptographicHash::Sha256);
    hashBuilder.addData(QByteArrayView(
        reinterpret_cast<const char*>(background.rgba.data()),
        static_cast<qsizetype>(background.rgba.size())));
    const QByteArray hash = hashBuilder.result();
    std::copy_n(reinterpret_cast<const std::uint8_t*>(hash.constData()),
                background.contentHash.size(), background.contentHash.begin());
    background.sourceProvenance = fileInfo.fileName().toStdString();
    std::vector<std::uint8_t> canonical;
    std::string canonicalError;
    if (!encodeCanonicalRgbaPng(background.width, background.height,
                                background.rgba, canonical,
                                canonicalError)) {
        error = QString::fromStdString(canonicalError);
        return false;
    }
    return true;
}

QString overthrustBackgroundRequirements(const std::size_t materialWidth,
                                         const std::size_t materialHeight)
{
    if (materialWidth == 0 || materialHeight == 0) return {};
    const std::size_t maximumWidth =
        overthrustMaximumBackgroundDimension(materialWidth);
    const std::size_t maximumHeight =
        overthrustMaximumBackgroundDimension(materialHeight);
    return QObject::tr("Maximum size: %1 x %2 px")
        .arg(maximumWidth)
        .arg(maximumHeight);
}

void rebuildTeamModeSettings(OverThrustProjectData& data)
{
    std::vector<OverThrustTeamBinding> teams;
    for (const OverThrustObject& object : data.objects) {
        if (object.type == "team_base") {
            teams.push_back({object.teamSlot, object.id});
        }
    }
    std::sort(teams.begin(), teams.end(),
              [](const OverThrustTeamBinding& left,
                 const OverThrustTeamBinding& right) {
                  return left.teamSlot < right.teamSlot;
              });
    data.teamModeSettings.clear();
    for (const std::string& mode : data.allowedGameModes) {
        if (mode == "core:team_deathmatch" ||
            mode == "core:capture_the_flag") {
            data.teamModeSettings.push_back({mode, teams});
        }
    }
}

QString readResourceText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Unable to load %1").arg(path);
    }
    return QString::fromUtf8(file.readAll());
}

QTextBrowser* markdownBrowser(const QString& resourcePath, QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setMarkdown(readResourceText(resourcePath));
    browser->setOpenExternalLinks(true);
    return browser;
}

QTextBrowser* plainTextBrowser(const QString& resourcePath, QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setPlainText(readResourceText(resourcePath));
    return browser;
}

QString materialDescription(const GameId game, const int paletteIndex)
{
    const int index = colorChartNumber(paletteIndex);
    if (game == GameId::OverThrust) {
        const OverThrustMaterial* material = overthrustMaterial(
            static_cast<std::uint8_t>(index));
        if (material == nullptr) {
            return QStringLiteral("Unmapped; cannot be published");
        }
        QString mechanics = QStringLiteral("Game-owned material behavior");
        if (material->materialId == "core:empty") {
            mechanics = material->category == "fly_through"
                            ? QStringLiteral(
                                  "Empty/fly-through simulation cell; color is visual only")
                            : QStringLiteral("Empty simulation cell");
        } else if (material->materialId == "core:fertile_soil") {
            mechanics = QStringLiteral("Schedules deterministic upward grass growth");
        } else if (material->materialId == "core:bedrock") {
            mechanics = QStringLiteral("Indestructible structural material");
        } else if (material->materialId == "core:aluminium") {
            mechanics = QStringLiteral("Conductive; stage-2 heat can produce molten aluminium");
        } else if (material->materialId == "core:titanium") {
            mechanics = QStringLiteral("Highly resistant and conductive; slowly erodes at stage 2");
        } else if (material->materialId == "core:tungsten") {
            mechanics = QStringLiteral("Indestructible, conductive, and thermally immune");
        } else if (material->materialId == "core:water") {
            mechanics = QStringLiteral("Suppresses fire/coatings and cools lava into steam");
        } else if (material->materialId == "core:oil") {
            mechanics = QStringLiteral("Flammable liquid that can coat actors");
        } else if (material->materialId == "core:napalm") {
            mechanics = QStringLiteral("Slow flammable liquid with a strong coating");
        } else if (material->materialId == "core:lava") {
            mechanics = QStringLiteral("Slow damaging liquid and stage-2 heat source");
        } else if (material->materialId == "core:smoke") {
            mechanics = QStringLiteral("Gas concentration reduces visibility");
        } else if (material->materialId == "core:acidic_gas") {
            mechanics = QStringLiteral(
                "Acidic gas seed; game-owned rules apply corrosive effects");
        } else if (material->materialId == "core:base_pad") {
            mechanics = QStringLiteral("Solid contact surface using the game-owned healing profile");
        }
        return QStringLiteral("%1\nMaterial: %2\nAppearance: %3\nCategory: %4\n%5")
            .arg(QString::fromUtf8(material->label.data(),
                                   material->label.size()),
                 QString::fromUtf8(material->materialId.data(),
                                   material->materialId.size()),
                 QString::fromUtf8(material->appearanceId.data(),
                                   material->appearanceId.size()),
                 QString::fromUtf8(material->category.data(),
                                   material->category.size()),
                 mechanics);
    }
    if (isReservedPaletteIndex(game, paletteIndex)) {
        return QStringLiteral("Reserved (do not use)");
    }
    if (game == GameId::Auts) {
        if (index >= 92 && index <= 95) {
            return QStringLiteral("Docking plate");
        }
        switch (index) {
        case 0: return QStringLiteral("Space");
        case 7: return QStringLiteral("Indestructible");
        case 39:
            return QStringLiteral(
                "Water (maximum 7 short surfaces per level)");
        case 108:
        case 109:
        case 110:
        case 111:
            return QStringLiteral(
                "Gray terrain (do not mix with docking plate colors 92-95)");
        default: return QStringLiteral("Normal terrain / color");
        }
    }
    if (game == GameId::Wings) {
        if (index >= 32 && index <= 37) {
            return QStringLiteral("Base");
        }
        if (index >= 38 && index <= 39) {
            return QStringLiteral("Indestructible base");
        }
        if (index >= 40 && index <= 47) {
            return QStringLiteral("Team base");
        }
        if (index >= 64 && index <= 79) {
            return QStringLiteral("Fly-through background");
        }
        if (index >= 80 && index <= 95) {
            return QStringLiteral("Indestructible terrain");
        }
        if (index >= 96 && index <= 111) {
            return QStringLiteral("Soft terrain");
        }
        if (index >= 112 && index <= 127) {
            return QStringLiteral("Burning terrain");
        }
        if (index >= 128) {
            return QStringLiteral("Normal terrain");
        }
        switch (index) {
        case 0: return QStringLiteral("Background");
        case 16:
            return QStringLiteral(
                "Creates water (fixed palette color 0-47)");
        case 48: return QStringLiteral("Water");
        case 49: return QStringLiteral("Water flow down");
        case 50: return QStringLiteral("Water flow left");
        case 51: return QStringLiteral("Water flow right");
        case 52:
            return QStringLiteral("Bubbles (editable bubble effect color)");
        case 53:
            return QStringLiteral("Snow (editable snow color)");
        case 54: return QStringLiteral("Damaging fire background");
        case 55:
        case 56: return QStringLiteral("Explosive terrain");
        default: return QStringLiteral("Unknown / undocumented");
        }
    }
    if (index >= 20 && index <= 30) {
        return QStringLiteral("Fly through");
    }
    if (index >= 32 && index <= 37) {
        return QStringLiteral("Font");
    }
    if (index >= 40 && index <= 42) {
        return QStringLiteral("Explodes into grenade-launcher shells");
    }
    if (index >= 43 && index <= 44) {
        return QStringLiteral("Explodes like plastic explosive");
    }
    if (index >= 53 && index <= 55) {
        return QStringLiteral("Reserved for fire (do not use)");
    }
    if (index >= 57 && index <= 149) {
        return QStringLiteral("Normal terrain");
    }
    if (index >= 151 && index <= 174) {
        return QStringLiteral("Burns away");
    }
    if (index >= 176 && index <= 199) {
        return QStringLiteral("Burns to ash");
    }
    if (index >= 204 && index <= 219) {
        return QStringLiteral("Turns into water when destroyed");
    }
    if (index >= 221 && index <= 243) {
        return QStringLiteral("Indestructible");
    }
    if (index >= 248 && index <= 255) {
        return QStringLiteral("Indestructible");
    }

    switch (index) {
    case 0:
        return QStringLiteral("Background (always black)");
    case 16:
        return QStringLiteral("Water");
    case 17:
        return QStringLiteral("Waterfall (flows down)");
    case 18:
        return QStringLiteral("Water (flows right)");
    case 19:
        return QStringLiteral("Water (flows left)");
    case 39:
        return QStringLiteral("Ice (not in water; shared game color)");
    case 45:
        return QStringLiteral("Plastic explosive");
    case 46:
        return QStringLiteral("Bird color (changes flying birds globally)");
    case 48:
        return QStringLiteral("Blood color (also changes pilot blood)");
    case 49:
        return QStringLiteral("Clay");
    case 50:
        return QStringLiteral("Base");
    case 51:
        return QStringLiteral("Ash color (shared game color)");
    case 52:
        return QStringLiteral("Snow color (shared game color)");
    case 56:
        return QStringLiteral("Bubble color (shared game color)");
    case 150:
        return QStringLiteral("Does not burn");
    case 175:
        return QStringLiteral("Burns forever (do not use)");
    case 200:
    case 220:
        return QStringLiteral(
            "Indestructible, does not damage Ghostship (do not use)");
    case 201:
        return QStringLiteral("Underwater ash");
    case 202:
        return QStringLiteral("Underwater clay");
    case 203:
        return QStringLiteral("Underwater ice (shared game color)");
    case 244:
        return QStringLiteral(
            "Turret firing-muzzle color (does not place a turret)");
    case 245:
        return QStringLiteral(
            "Turret barrel/body color (does not place a turret)");
    case 246:
        return QStringLiteral(
            "Turret muzzle color (does not place a turret)");
    case 247:
        return QStringLiteral(
            "Turret side color (does not place a turret)");
    default:
        return QStringLiteral("Unknown / undocumented");
    }
}

class ObjectListCommand final : public QUndoCommand {
public:
    ObjectListCommand(std::vector<OverThrustObject>* objects,
                      std::vector<OverThrustObject> before,
                      std::vector<OverThrustObject> after,
                      std::function<void()> refresh, const QString& text)
        : objects_(objects), before_(std::move(before)),
          after_(std::move(after)), refresh_(std::move(refresh))
    {
        setText(text);
    }

    void undo() override
    {
        *objects_ = before_;
        refresh_();
    }

    void redo() override
    {
        *objects_ = after_;
        refresh_();
    }

private:
    std::vector<OverThrustObject>* objects_;
    std::vector<OverThrustObject> before_;
    std::vector<OverThrustObject> after_;
    std::function<void()> refresh_;
};

QIcon toolIcon(const DrawTool tool)
{
    switch (tool) {
    case DrawTool::Pencil:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_pen.png"));
    case DrawTool::Eraser:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_eraser.png"));
    case DrawTool::Line:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_line.png"));
    case DrawTool::Rectangle:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_rectangle.png"));
    case DrawTool::FloodFill:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_flood_fill.png"));
    case DrawTool::Eyedropper:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_color_picker.png"));
    case DrawTool::Ellipse:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_ellipse.png"));
    case DrawTool::Spray:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_spraycan.png"));
    case DrawTool::Text:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_text.png"));
    case DrawTool::Polygon:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_polygon.png"));
    case DrawTool::SelectRectangle:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_rect_selection.png"));
    case DrawTool::SelectEllipse:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_elliptical_selection.png"));
    case DrawTool::SelectFreehand:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_free_form_selection.png"));
    case DrawTool::MagicWand:
        return QIcon(QStringLiteral(":/icons/icons/magic-wand.svg"));
    case DrawTool::MoveSelection:
        return QIcon(QStringLiteral(":/icons/icons/hand-move.svg"));
    case DrawTool::BezierCurve:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_curve.png"));
    }
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), level_(std::make_unique<Level>())
{
    level_->name = "UNTITLED";
    level_->palette = defaultVWingPalette();
    level_->pixels.fill(0);

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    documentTabs_ = new QTabBar(central);
    documentTabs_->addTab(tr("Level"));
    documentTabs_->addTab(tr("Background"));
    documentTabs_->setExpanding(false);
    documentTabs_->setVisible(false);
    centralLayout->addWidget(documentTabs_);
    canvas_ = new LevelCanvas(central);
    canvas_->setLevel(level_.get());
    centralLayout->addWidget(canvas_, 1);
    setCentralWidget(central);

    createActions();
    createToolBars();
    createMaterialDock();
    createLayerDock();
    createObjectDock();
    createZoomToolBar();

    positionLabel_ = new QLabel(tr("Ready"), this);
    statusBar()->addPermanentWidget(positionLabel_);
    connect(canvas_, &LevelCanvas::cursorPositionChanged, this,
            [this](const int x, const int y, const int index) {
                positionLabel_->setText(
                    tr("x=%1  y=%2  index=%3").arg(x).arg(y).arg(index));
            });
    connect(canvas_, &LevelCanvas::cursorLeftCanvas, this,
            [this] { positionLabel_->setText(tr("Ready")); });
    connect(canvas_, &LevelCanvas::undoCleanChanged, this,
            [this](const bool clean) {
                Q_UNUSED(clean);
                setModified(nonUndoModified_ || canvas_->hasDirtyUndoStack() ||
                            canvas_->hasPendingSelectionEdit());
            });
    connect(canvas_, &LevelCanvas::pendingSelectionEditChanged, this,
            [this](const bool pending) {
                setModified(nonUndoModified_ || pending ||
                            !canvas_->undoStack()->isClean());
            });
    connect(canvas_, &LevelCanvas::layersChanged, this,
            &MainWindow::refreshLayerList);
    connect(canvas_, &LevelCanvas::objectSelectionChanged, this,
            [this](const int index) {
                if (objectListWidget_ == nullptr) return;
                objectListWidget_->setCurrentRow(index);
            });
    connect(canvas_, &LevelCanvas::objectTransformRequested, this,
            [this](const int index, const int x, const int y,
                   const int width, const int height,
                   const int rotationMillidegrees) {
                if (!overthrustData_ || index < 0 ||
                    index >= static_cast<int>(overthrustData_->objects.size())) {
                    return;
                }
                auto before = overthrustData_->objects;
                auto after = before;
                OverThrustObject& moved =
                    after[static_cast<std::size_t>(index)];
                const int oldWidth = moved.transform.width;
                int slideDistance = static_cast<int>(std::round(std::hypot(
                    moved.openOffsetX, moved.openOffsetY)));
                const int deltaX = x - moved.transform.x;
                const int deltaY = y - moved.transform.y;
                moved.transform.x = x;
                moved.transform.y = y;
                moved.transform.width = width;
                moved.transform.height = height;
                moved.transform.rotationMillidegrees =
                    rotationMillidegrees;
                if (moved.type == "sliding_door") {
                    if (slideDistance <= 0) slideDistance = oldWidth;
                    if (slideDistance == oldWidth && width != oldWidth) {
                        slideDistance = width;
                    }
                    const double radians = rotationMillidegrees *
                        3.14159265358979323846 / 180000.0;
                    moved.openOffsetX = static_cast<int>(std::round(
                        slideDistance * std::cos(radians)));
                    moved.openOffsetY = static_cast<int>(std::round(
                        slideDistance * std::sin(radians)));
                }
                for (OverThrustPoint& point : moved.areaPolygon) {
                    point.x += deltaX;
                    point.y += deltaY;
                }
                applyObjectListChange(tr("Transform object"),
                                      std::move(before), std::move(after));
            });
    connect(canvas_, &LevelCanvas::teamBasePolygonRequested, this,
            &MainWindow::applyTeamBasePolygon);
    connect(canvas_, &LevelCanvas::teamBasePolygonActiveChanged, this,
            [this](const bool active) {
                if (!active) statusBar()->clearMessage();
            });
    connect(documentTabs_, &QTabBar::currentChanged, this,
            &MainWindow::switchProfileDocument);

    resize(1000, 800);
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    // Child widgets are destroyed by QMainWindow after this class's members.
    // A dirty undo stack emits cleanChanged while LevelCanvas is torn down;
    // disconnect it before level_ and the other title data cease to exist.
    if (canvas_ != nullptr) {
        QObject::disconnect(canvas_, nullptr, this, nullptr);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::promptForInitialLevel()
{
    createNewLevel(false);
}

void MainWindow::createActions()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* newAction = fileMenu->addAction(tr("&New level"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newLevel);

    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openLevel);

    QAction* importAction = fileMenu->addAction(tr("&Import..."));
    connect(importAction, &QAction::triggered, this,
            &MainWindow::importImage);

    QAction* exportPcxAction =
        fileMenu->addAction(tr("Export current image as &PCX..."));
    connect(exportPcxAction, &QAction::triggered, this,
            &MainWindow::exportPcx);

    saveAction_ = fileMenu->addAction(tr("&Save project"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this,
            [this] { saveProject(); });

    QAction* saveAsAction = fileMenu->addAction(tr("Save project &as..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this,
            [this] { saveProjectAs(); });

    publishAction_ = fileMenu->addAction(tr("&Publish LEV..."));
    connect(publishAction_, &QAction::triggered, this,
            [this] { publishLevel(); });

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] {
        canvas_->commitSelection();
        canvas_->undoStack()->undo();
    });
    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this] {
        canvas_->commitSelection();
        canvas_->undoStack()->redo();
    });
    editMenu->addSeparator();
    QAction* selectAllAction = editMenu->addAction(tr("Select &all"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, canvas_,
            &LevelCanvas::selectAll);
    QAction* copyAction = editMenu->addAction(tr("&Copy selection"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, canvas_,
            &LevelCanvas::copySelection);
    QAction* pasteAction = editMenu->addAction(tr("&Paste selection"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, canvas_,
            &LevelCanvas::pasteSelection);
    QAction* deleteAction = editMenu->addAction(tr("&Delete selection"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this] {
        if (creationSettings_.game == GameId::OverThrust &&
            documentTabs_->currentIndex() == 0 &&
            objectModeCheck_ != nullptr && objectModeCheck_->isChecked()) {
            deleteSelectedOverThrustObject();
        } else {
            canvas_->deleteSelection();
        }
    });

    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    QAction* applicationSettingsAction =
        settingsMenu->addAction(tr("&Preferences..."));
    connect(applicationSettingsAction, &QAction::triggered, this,
            &MainWindow::editApplicationSettings);

    QMenu* levelMenu = menuBar()->addMenu(tr("&Level"));
    levelSettingsAction_ =
        levelMenu->addAction(tr("Wings level &settings..."));
    levelSettingsAction_->setEnabled(false);
    connect(levelSettingsAction_, &QAction::triggered, this,
            &MainWindow::editLevelSettings);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* userGuideAction = helpMenu->addAction(tr("&User guide..."));
    userGuideAction->setShortcut(QKeySequence::HelpContents);
    connect(userGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("PCX Level Tool user guide"),
                             QStringLiteral(":/help/user-guide-fi.md"));
    });

    QMenu* gameGuidesMenu = helpMenu->addMenu(tr("&Game guides"));
    QAction* vwingGuideAction = gameGuidesMenu->addAction(tr("&V-Wing..."));
    connect(vwingGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("V-Wing level guide"),
                             QStringLiteral(":/help/game-guide-vwing-fi.md"));
    });
    QAction* wingsGuideAction = gameGuidesMenu->addAction(tr("&Wings..."));
    connect(wingsGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("Wings level guide"),
                             QStringLiteral(":/help/game-guide-wings-fi.md"));
    });
    QAction* autsGuideAction = gameGuidesMenu->addAction(tr("&AUTS..."));
    connect(autsGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("AUTS level guide"),
                             QStringLiteral(":/help/game-guide-auts-fi.md"));
    });
    QAction* overthrustGuideAction =
        gameGuidesMenu->addAction(tr("&OverThrust..."));
    connect(overthrustGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(
            tr("OverThrust level guide"),
            QStringLiteral(":/help/game-guide-overthrust-fi.md"));
    });

    helpMenu->addSeparator();
    QAction* licensesAction = helpMenu->addAction(tr("&Licenses..."));
    connect(licensesAction, &QAction::triggered, this,
            &MainWindow::showLicenses);
}

void MainWindow::showMarkdownDocument(const QString& title,
                                      const QString& resourcePath)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(markdownBrowser(resourcePath, &dialog), 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(850, 700);
    dialog.exec();
}

void MainWindow::showLicenses()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("PCX Level Tool licenses"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget(&dialog);
    tabs->addTab(markdownBrowser(QStringLiteral(":/help/licenses-fi.md"), tabs),
                 tr("Summary"));
    tabs->addTab(plainTextBrowser(QStringLiteral(":/help/LICENSE"), tabs),
                 tr("PCX Level Tool"));
    tabs->addTab(markdownBrowser(
                     QStringLiteral(":/help/THIRD_PARTY_NOTICES.md"), tabs),
                 tr("Third-party"));
    tabs->addTab(markdownBrowser(
                     QStringLiteral(":/help/QT-LGPL-NOTICE.md"), tabs),
                 tr("Qt"));
    tabs->addTab(plainTextBrowser(QStringLiteral(":/help/CONVERT.TXT"), tabs),
                 tr("V-Wing converter"));
    layout->addWidget(tabs, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(850, 700);
    dialog.exec();
}

void MainWindow::editApplicationSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("PCX Level Tool preferences"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(
        tr("Choose the default project folder and the folder used when "
           "publishing for each game. You can still choose a "
           "different location in every save dialog."),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* group = new QGroupBox(tr("Default folders"), &dialog);
    auto* form = new QFormLayout(group);
    struct DirectoryEditor {
        GameId game;
        QLineEdit* edit;
    };
    std::vector<DirectoryEditor> editors;
    QSettings settings;

    auto* projectRow = new QWidget(group);
    auto* projectRowLayout = new QHBoxLayout(projectRow);
    projectRowLayout->setContentsMargins(0, 0, 0, 0);
    auto* projectEdit = new QLineEdit(projectRow);
    projectEdit->setText(QDir::toNativeSeparators(
        effectiveProjectDirectorySetting(settings)));
    auto* projectBrowse = new QPushButton(tr("Browse..."), projectRow);
    auto* projectDefault = new QPushButton(tr("Default"), projectRow);
    projectRowLayout->addWidget(projectEdit, 1);
    projectRowLayout->addWidget(projectBrowse);
    projectRowLayout->addWidget(projectDefault);
    form->addRow(tr("Projects (.pxlp):"), projectRow);
    connect(projectBrowse, &QPushButton::clicked, &dialog,
            [&dialog, projectEdit] {
                const QString directory = QFileDialog::getExistingDirectory(
                    &dialog, tr("Select PCX Level Tool project folder"),
                    projectEdit->text());
                if (!directory.isEmpty()) {
                    projectEdit->setText(QDir::toNativeSeparators(directory));
                }
            });
    connect(projectDefault, &QPushButton::clicked, &dialog,
            [projectEdit] {
                projectEdit->setText(QDir::toNativeSeparators(
                    defaultProjectDirectory()));
            });

    for (const GameProfile& profile : availableGameProfiles()) {
        auto* row = new QWidget(group);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* edit = new QLineEdit(row);
        edit->setText(QDir::toNativeSeparators(
            exportDirectorySetting(settings, profile.id)));
        edit->setPlaceholderText(tr("Not set"));
        auto* browse = new QPushButton(tr("Browse..."), row);
        auto* clear = new QPushButton(tr("Clear"), row);
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        rowLayout->addWidget(clear);

        const QString gameName =
            QString::fromUtf8(profile.displayName.data(),
                              static_cast<qsizetype>(profile.displayName.size()));
        form->addRow(tr("%1 publish folder:").arg(gameName), row);
        connect(browse, &QPushButton::clicked, &dialog,
                [&dialog, edit, gameName] {
                    const QString directory = QFileDialog::getExistingDirectory(
                        &dialog,
                        tr("Select %1 publish folder").arg(gameName),
                        edit->text());
                    if (!directory.isEmpty()) {
                        edit->setText(QDir::toNativeSeparators(directory));
                    }
                });
        connect(clear, &QPushButton::clicked, edit, &QLineEdit::clear);
        editors.push_back({profile.id, edit});
    }
    layout->addWidget(group);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    setProjectDirectorySetting(settings, projectEdit->text());
    for (const DirectoryEditor& editor : editors) {
        setExportDirectorySetting(settings, editor.game, editor.edit->text());
    }
    statusBar()->showMessage(tr("Preferences saved"), 3000);
}

void MainWindow::createToolBars()
{
    auto* toolBar = new QToolBar(tr("Tools"), this);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, toolBar);
    auto* toolGroup = new QButtonGroup(toolBar);
    toolGroup->setExclusive(true);
    const std::array<std::pair<const char*, DrawTool>, 16> tools{{
        {"Pencil", DrawTool::Pencil},
        {"Eraser", DrawTool::Eraser},
        {"Spray", DrawTool::Spray},
        {"Line", DrawTool::Line},
        {"Bezier curve", DrawTool::BezierCurve},
        {"Rectangle", DrawTool::Rectangle},
        {"Ellipse", DrawTool::Ellipse},
        {"Polygon", DrawTool::Polygon},
        {"Flood fill", DrawTool::FloodFill},
        {"Eyedropper", DrawTool::Eyedropper},
        {"Text", DrawTool::Text},
        {"Rectangle select", DrawTool::SelectRectangle},
        {"Ellipse select", DrawTool::SelectEllipse},
        {"Freehand select", DrawTool::SelectFreehand},
        {"Magic wand", DrawTool::MagicWand},
        {"Move selection", DrawTool::MoveSelection},
    }};
    for (const auto& [label, tool] : tools) {
        auto* button = new QToolButton(toolBar);
        button->setCheckable(true);
        button->setIcon(toolIcon(tool));
        button->setIconSize(QSize(28, 28));
        button->setFixedSize(38, 38);
        button->setStyleSheet(QStringLiteral(
            "QToolButton { background: #f5f5f5; border: 1px solid #808080; }"
            "QToolButton:hover { background: #e8f2ff; }"
            "QToolButton:checked { background: #b9d9ff; border: 2px inset "
            "#5078a0; }"));
        button->setToolTip(tr(label));
        if (tool == DrawTool::Text) {
            button->setToolTip(tr("Text: drag an area, type directly, drag "
                                  "the box to move it; Ctrl+Enter or clicking "
                                  "outside accepts"));
        } else if (tool == DrawTool::Polygon) {
            button->setToolTip(tr("Polygon: click corners, double-click or "
                                  "press Enter to finish"));
        } else if (tool == DrawTool::Line) {
            button->setToolTip(
                tr("Line: hold Shift for horizontal, vertical, or 45-degree "
                   "directions"));
        } else if (tool == DrawTool::BezierCurve) {
            button->setToolTip(
                tr("Bezier curve: hold Shift to constrain only the initial "
                   "line; control-point adjustments remain free"));
        } else if (tool == DrawTool::MagicWand) {
            button->setToolTip(
                tr("Magic wand: select matching material on the active "
                   "layer; Shift adds, Ctrl subtracts, and Shift+Ctrl "
                   "intersects"));
        } else if (tool == DrawTool::SelectRectangle ||
                   tool == DrawTool::SelectEllipse ||
                   tool == DrawTool::SelectFreehand) {
            button->setToolTip(
                tr("Selection: Shift adds, Ctrl subtracts, Shift+Ctrl "
                   "intersects with the current selection"));
        } else if (tool == DrawTool::Eraser) {
            button->setToolTip(tr("Eraser: makes upper layers transparent; "
                                  "writes file index 0 on "
                                  "Background"));
        }
        button->setAccessibleName(tr(label));
        toolGroup->addButton(button, static_cast<int>(tool));
        toolBar->addWidget(button);
        if (tool == DrawTool::Pencil) {
            button->setChecked(true);
        }
    }
    addToolBarBreak(Qt::TopToolBarArea);
    auto* optionsBar = new QToolBar(tr("Tool options"), this);
    optionsBar->setMovable(false);
    optionsBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, optionsBar);
    auto* activeToolLabel = new QLabel(tr("Pencil"), optionsBar);
    activeToolLabel->setMinimumWidth(90);
    optionsBar->addWidget(activeToolLabel);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Thickness: "), optionsBar));
    auto* thicknessSpinBox = new QSpinBox(optionsBar);
    thicknessSpinBox->setRange(1, 32);
    thicknessSpinBox->setSuffix(tr(" px"));
    thicknessSpinBox->setValue(canvas_->toolThickness(DrawTool::Pencil));
    optionsBar->addWidget(thicknessSpinBox);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Tip: "), optionsBar));
    auto* brushShapeCombo = new QComboBox(optionsBar);
    brushShapeCombo->addItem(tr("Square"),
                             static_cast<int>(BrushShape::Square));
    brushShapeCombo->addItem(tr("Circle"),
                             static_cast<int>(BrushShape::Circle));
    brushShapeCombo->setToolTip(
        tr("Pencil and eraser footprint shown under the pointer"));
    optionsBar->addWidget(brushShapeCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Match: "), optionsBar));
    auto* materialSelectionCombo = new QComboBox(optionsBar);
    materialSelectionCombo->addItem(
        tr("Connected only"),
        static_cast<int>(MaterialSelectionMode::Connected));
    materialSelectionCombo->addItem(
        tr("All same material"),
        static_cast<int>(MaterialSelectionMode::AllMatching));
    materialSelectionCombo->setToolTip(
        tr("Magic wand matching within the active layer"));
    materialSelectionCombo->setEnabled(false);
    optionsBar->addWidget(materialSelectionCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Shape: "), optionsBar));
    auto* shapeModeCombo = new QComboBox(optionsBar);
    shapeModeCombo->addItem(tr("Outline"), static_cast<int>(ShapeMode::Outline));
    shapeModeCombo->addItem(tr("Outline + fill"),
                            static_cast<int>(ShapeMode::OutlineAndFill));
    shapeModeCombo->addItem(tr("Fill only"),
                            static_cast<int>(ShapeMode::FillOnly));
    shapeModeCombo->setEnabled(false);
    optionsBar->addWidget(shapeModeCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Corners: "), optionsBar));
    auto* cornerRadiusCombo = new QComboBox(optionsBar);
    cornerRadiusCombo->addItem(tr("Sharp"), 0);
    cornerRadiusCombo->addItem(tr("2 px"), 2);
    cornerRadiusCombo->addItem(tr("4 px"), 4);
    cornerRadiusCombo->addItem(tr("8 px"), 8);
    cornerRadiusCombo->addItem(tr("16 px"), 16);
    cornerRadiusCombo->addItem(tr("32 px"), 32);
    cornerRadiusCombo->setEnabled(false);
    optionsBar->addWidget(cornerRadiusCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Font: "), optionsBar));
    auto* fontCombo = new QFontComboBox(optionsBar);
    fontCombo->setEnabled(false);
    fontCombo->setMaximumWidth(180);
    optionsBar->addWidget(fontCombo);
    auto* textSizeSpinBox = new QSpinBox(optionsBar);
    textSizeSpinBox->setRange(6, 64);
    textSizeSpinBox->setValue(12);
    textSizeSpinBox->setSuffix(tr(" px"));
    textSizeSpinBox->setEnabled(false);
    optionsBar->addWidget(textSizeSpinBox);
    connect(
        toolGroup, &QButtonGroup::idClicked, this,
        [this, activeToolLabel, thicknessSpinBox, cornerRadiusCombo,
         brushShapeCombo, materialSelectionCombo, shapeModeCombo, fontCombo,
         textSizeSpinBox, tools](const int id) {
            const auto selectedTool = static_cast<DrawTool>(id);
            canvas_->setDrawTool(selectedTool);
            for (const auto& [label, tool] : tools) {
                if (static_cast<int>(tool) == id) {
                    activeToolLabel->setText(tr(label));
                    break;
                }
            }
            const bool supportsThickness =
                selectedTool == DrawTool::Pencil ||
                selectedTool == DrawTool::Eraser ||
                selectedTool == DrawTool::Line ||
                selectedTool == DrawTool::Rectangle ||
                selectedTool == DrawTool::Ellipse ||
                selectedTool == DrawTool::Polygon ||
                selectedTool == DrawTool::Spray ||
                selectedTool == DrawTool::BezierCurve;
            const QSignalBlocker blocker(thicknessSpinBox);
            thicknessSpinBox->setEnabled(supportsThickness);
            thicknessSpinBox->setValue(canvas_->toolThickness(selectedTool));
            const bool supportsBrushShape = selectedTool == DrawTool::Pencil ||
                                            selectedTool == DrawTool::Eraser;
            brushShapeCombo->setEnabled(supportsBrushShape);
            if (supportsBrushShape) {
                const QSignalBlocker brushShapeBlocker(brushShapeCombo);
                brushShapeCombo->setCurrentIndex(brushShapeCombo->findData(
                    static_cast<int>(canvas_->brushShape(selectedTool))));
            }
            const bool supportsMaterialSelection =
                selectedTool == DrawTool::MagicWand;
            materialSelectionCombo->setEnabled(supportsMaterialSelection);
            if (supportsMaterialSelection) {
                const QSignalBlocker materialSelectionBlocker(
                    materialSelectionCombo);
                materialSelectionCombo->setCurrentIndex(
                    materialSelectionCombo->findData(static_cast<int>(
                        canvas_->materialSelectionMode())));
            }
            const bool supportsCorners = selectedTool == DrawTool::Rectangle;
            cornerRadiusCombo->setEnabled(supportsCorners);
            if (supportsCorners) {
                cornerRadiusCombo->setCurrentIndex(cornerRadiusCombo->findData(
                    canvas_->rectangleCornerRadius()));
            }
            const bool supportsFill = selectedTool == DrawTool::Rectangle ||
                                      selectedTool == DrawTool::Ellipse ||
                                      selectedTool == DrawTool::Polygon;
            shapeModeCombo->setEnabled(supportsFill);
            const bool supportsText = selectedTool == DrawTool::Text;
            fontCombo->setEnabled(supportsText);
            textSizeSpinBox->setEnabled(supportsText);
        });
    connect(thicknessSpinBox, &QSpinBox::valueChanged, this,
            [this, toolGroup](const int value) {
                canvas_->setToolThickness(
                    static_cast<DrawTool>(toolGroup->checkedId()), value);
            });
    connect(brushShapeCombo, &QComboBox::currentIndexChanged, this,
            [this, toolGroup, brushShapeCombo](const int index) {
                canvas_->setBrushShape(
                    static_cast<DrawTool>(toolGroup->checkedId()),
                    static_cast<BrushShape>(
                        brushShapeCombo->itemData(index).toInt()));
            });
    connect(materialSelectionCombo, &QComboBox::currentIndexChanged, this,
            [this, materialSelectionCombo](const int index) {
                canvas_->setMaterialSelectionMode(
                    static_cast<MaterialSelectionMode>(
                        materialSelectionCombo->itemData(index).toInt()));
            });
    connect(cornerRadiusCombo, &QComboBox::currentIndexChanged, this,
            [this, cornerRadiusCombo](const int index) {
                canvas_->setRectangleCornerRadius(
                    cornerRadiusCombo->itemData(index).toInt());
            });
    connect(shapeModeCombo, &QComboBox::currentIndexChanged, this,
            [this, shapeModeCombo](const int index) {
                const auto mode = static_cast<ShapeMode>(
                    shapeModeCombo->itemData(index).toInt());
                canvas_->setShapeMode(mode);
            });
    connect(fontCombo, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font) {
                canvas_->setTextFontFamily(font.family());
            });
    canvas_->setTextFontFamily(fontCombo->currentFont().family());
    connect(textSizeSpinBox, &QSpinBox::valueChanged, canvas_,
            &LevelCanvas::setTextPixelSize);
}

void MainWindow::createMaterialDock()
{
    auto* dock = new QDockWidget(tr("Palette"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(tr("Palette"), contents));
    paletteGroupCombo_ = new QComboBox(contents);
    const std::array<std::pair<const char*, PaletteGroup>, 11> paletteGroups{{
        {"All documented usable", PaletteGroup::AllUsable},
        {"Background (0)", PaletteGroup::Background},
        {"Water (16-19)", PaletteGroup::Water},
        {"Fly through (20-30)", PaletteGroup::FlyThrough},
        {"Font (32-37)", PaletteGroup::Font},
        {"Special materials and game colors (39-56)",
         PaletteGroup::Special},
        {"Normal terrain (57-149)", PaletteGroup::NormalTerrain},
        {"Burnable (150-199)", PaletteGroup::Burnable},
        {"Underwater (201-219)", PaletteGroup::Underwater},
        {"Indestructible (221-243, 248-255)", PaletteGroup::Indestructible},
        {"Turret colors (244-247; not placement)", PaletteGroup::Turrets},
    }};
    for (const auto& [label, group] : paletteGroups) {
        paletteGroupCombo_->addItem(tr(label), static_cast<int>(group));
    }
    layout->addWidget(paletteGroupCombo_);
    backgroundPreviewCheck_ = new QCheckBox(
        tr("Show imported PNG background behind materials"), contents);
    backgroundPreviewCheck_->setChecked(false);
    backgroundPreviewCheck_->setVisible(false);
    backgroundPreviewCheck_->setToolTip(
        tr("Presentation preview only. Turn this off for a clear material-editing view."));
    layout->addWidget(backgroundPreviewCheck_);
    paletteWidget_ = new PaletteWidget(contents);
    paletteWidget_->setLevel(level_.get());
    paletteWidget_->setIndices(
        paletteIndices(PaletteGroup::AllUsable, GameId::VWing));
    layout->addWidget(paletteWidget_);

    auto* indexLayout = new QHBoxLayout();
    indexLayout->addWidget(new QLabel(tr("Left material index:"), contents));
    materialIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    materialIndexSpinBox_->setRange(0, 255);
    materialIndexSpinBox_->setValue(57);
    materialIndexSpinBox_->setToolTip(
        tr("Reserved material indices cannot be selected"));
    indexLayout->addWidget(materialIndexSpinBox_);
    layout->addLayout(indexLayout);
    auto* secondaryIndexLayout = new QHBoxLayout();
    secondaryIndexLayout->addWidget(
        new QLabel(tr("Right material index:"), contents));
    secondaryIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    secondaryIndexSpinBox_->setRange(0, 255);
    secondaryIndexSpinBox_->setValue(58);
    secondaryIndexSpinBox_->setToolTip(
        tr("Right-click drawing and filled-shape interior material index"));
    secondaryIndexLayout->addWidget(secondaryIndexSpinBox_);
    layout->addLayout(secondaryIndexLayout);
    materialDetailsLabel_ = new QLabel(contents);
    materialDetailsLabel_->setWordWrap(true);
    layout->addWidget(materialDetailsLabel_);
    editColorButton_ =
        new QPushButton(tr("Edit selected color..."), contents);
    layout->addWidget(editColorButton_);
    auto* paletteFileLayout = new QHBoxLayout();
    auto* loadPaletteButton = new QPushButton(tr("Load palette..."), contents);
    auto* savePaletteButton = new QPushButton(tr("Save palette..."), contents);
    paletteFileLayout->addWidget(loadPaletteButton);
    paletteFileLayout->addWidget(savePaletteButton);
    layout->addLayout(paletteFileLayout);
    layout->addStretch();
    connect(paletteGroupCombo_, &QComboBox::currentIndexChanged, this,
            [this](const int index) {
                auto indices = paletteIndices(static_cast<PaletteGroup>(
                    paletteGroupCombo_->itemData(index).toInt()),
                    creationSettings_.game);
                const auto selected = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(materialIndexSpinBox_->value()));
                const auto secondary = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(secondaryIndexSpinBox_->value()));
                const bool selectionVisible =
                    std::find(indices.begin(), indices.end(), selected) !=
                    indices.end();
                const bool secondaryVisible =
                    std::find(indices.begin(), indices.end(), secondary) !=
                    indices.end();
                const int firstIndex = indices.empty() ? 0 : indices.front();
                paletteWidget_->setIndices(std::move(indices));
                if (!selectionVisible) {
                    materialIndexSpinBox_->setValue(
                        colorChartNumber(firstIndex));
                }
                if (!secondaryVisible) {
                    secondaryIndexSpinBox_->setValue(
                        colorChartNumber(firstIndex));
                }
            });
    connect(backgroundPreviewCheck_, &QCheckBox::toggled, this,
            [this] { updateOverThrustBackgroundPreview(); });
    connect(materialIndexSpinBox_, &QSpinBox::valueChanged, this,
            [this](const int value) {
                const auto index = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(value));
                canvas_->setSelectedIndex(index);
                paletteWidget_->setSelectedIndex(index);
                updateMaterialDetails(index);
            });
    connect(canvas_, &LevelCanvas::selectedIndexChanged, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(secondaryIndexSpinBox_, &QSpinBox::valueChanged, this,
            [this](const int value) {
                const auto index = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(value));
                canvas_->setSecondaryIndex(index);
                paletteWidget_->setSecondaryIndex(index);
            });
    connect(canvas_, &LevelCanvas::secondaryIndexChanged, this,
            [this](const int index) {
                secondaryIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::indexSelected, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::secondaryIndexSelected, this,
            [this](const int index) {
                secondaryIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::indexEditRequested, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
                editSelectedPaletteColor();
            });
    connect(editColorButton_, &QPushButton::clicked, this,
            &MainWindow::editSelectedPaletteColor);
    connect(loadPaletteButton, &QPushButton::clicked, this,
            &MainWindow::loadPalette);
    connect(savePaletteButton, &QPushButton::clicked, this,
            &MainWindow::savePalette);
    connect(canvas_, &LevelCanvas::paletteColorChanged, this,
            [this](const int index) {
                if (creationSettings_.game == GameId::Wings &&
                    backgroundLevel_) {
                    Level* source = activeLevel();
                    Level* target = source == level_.get()
                                        ? backgroundLevel_.get()
                                        : level_.get();
                    if (index < 0) {
                        target->palette = source->palette;
                    } else {
                        target->palette[static_cast<std::size_t>(index)] =
                            source->palette[static_cast<std::size_t>(index)];
                    }
                }
                paletteWidget_->update();
                const int selectedIndex = paletteIndexFromColorChart(
                    materialIndexSpinBox_->value());
                if (index < 0 || index == selectedIndex) {
                    updateMaterialDetails(selectedIndex);
                }
            });
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));

    auto* scrollArea = new QScrollArea(dock);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(contents);
    dock->setWidget(scrollArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::createZoomToolBar()
{
    QToolBar* toolBar = addToolBar(tr("View"));
    levelNameLabel_ = new QLabel(tr("Level name: "), toolBar);
    toolBar->addWidget(levelNameLabel_);
    levelNameEdit_ = new QLineEdit(
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size())),
        toolBar);
    levelNameEdit_->setMaxLength(20);
    levelNameEdit_->setMaximumWidth(190);
    levelNameEdit_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[\\x20-\\x7E]{0,20}")),
        levelNameEdit_));
    levelNameEdit_->setToolTip(
        tr("Maximum 20 printable ASCII characters. Ä, Ö, Å and other "
           "non-ASCII characters are not supported by LEV files."));
    connect(levelNameEdit_, &QLineEdit::textEdited, this,
            [this](const QString& text) {
                const std::string name = text.toLatin1().toStdString();
                if (name == level_->name) {
                    return;
                }
                level_->name = name;
                creationSettings_.name = name;
                nonUndoModified_ = true;
                setModified(true);
            });
    toolBar->addWidget(levelNameEdit_);
    levelNameSeparatorAction_ = toolBar->addSeparator();
    toolBar->addWidget(new QLabel(tr("Zoom: "), toolBar));
    auto* zoomCombo = new QComboBox(toolBar);
    const std::array<std::pair<const char*, double>, 6> zoomLevels{{
        {"25%", 0.25},
        {"50%", 0.5},
        {"100%", 1.0},
        {"200%", 2.0},
        {"400%", 4.0},
        {"800%", 8.0},
    }};
    for (const auto& [label, value] : zoomLevels) {
        zoomCombo->addItem(QString::fromLatin1(label), value);
    }
    zoomCombo->setCurrentIndex(2);
    connect(zoomCombo, &QComboBox::currentIndexChanged, this,
            [this, zoomCombo](const int index) {
                canvas_->setZoom(zoomCombo->itemData(index).toDouble());
            });
    connect(canvas_, &LevelCanvas::zoomChanged, zoomCombo,
            [zoomCombo](const double zoom) {
                const QSignalBlocker blocker(zoomCombo);
                zoomCombo->setCurrentIndex(zoomCombo->findData(zoom));
            });
    toolBar->addWidget(zoomCombo);
    toolBar->addSeparator();
    auto* publishButton = new QPushButton(tr("Publish LEV..."), toolBar);
    publishButton->setToolTip(
        tr("Flatten visible layers and write a game-compatible LEV file"));
    connect(publishButton, &QPushButton::clicked, this,
            [this] { publishLevel(); });
    toolBar->addWidget(publishButton);
}

void MainWindow::createLayerDock()
{
    auto* dock = new QDockWidget(tr("Layers"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(
        tr("Top layer is drawn first. Unchecked layers are hidden."), contents));

    layerListWidget_ = new QListWidget(contents);
    layerListWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(layerListWidget_);

    auto* firstRow = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add"), contents);
    auto* duplicateButton = new QPushButton(tr("Duplicate"), contents);
    auto* mergeButton = new QPushButton(tr("Merge down"), contents);
    auto* deleteButton = new QPushButton(tr("Delete"), contents);
    firstRow->addWidget(addButton);
    firstRow->addWidget(duplicateButton);
    firstRow->addWidget(mergeButton);
    firstRow->addWidget(deleteButton);
    layout->addLayout(firstRow);

    auto* secondRow = new QHBoxLayout();
    auto* upButton = new QPushButton(tr("Up"), contents);
    auto* downButton = new QPushButton(tr("Down"), contents);
    auto* renameButton = new QPushButton(tr("Rename"), contents);
    auto* lockButton = new QPushButton(tr("Lock / unlock"), contents);
    secondRow->addWidget(upButton);
    secondRow->addWidget(downButton);
    secondRow->addWidget(renameButton);
    secondRow->addWidget(lockButton);
    layout->addLayout(secondRow);

    dock->setWidget(contents);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(layerListWidget_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current != nullptr) {
                    canvas_->setActiveLayer(
                        current->data(Qt::UserRole).toInt());
                }
            });
    connect(layerListWidget_, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* item) {
                const int index = item->data(Qt::UserRole).toInt();
                const bool visible = item->checkState() == Qt::Checked;
                Level* document = activeLevel();
                if (index > 0 &&
                    document->layers[static_cast<std::size_t>(index)].visible !=
                        visible) {
                    canvas_->setLayerVisible(index, visible);
                    nonUndoModified_ = true;
                    setModified(true);
                }
            });
    connect(addButton, &QPushButton::clicked, this, [this] {
        if (canvas_->addLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("A project can contain at most 5 layers"),
                                     3000);
        }
    });
    connect(duplicateButton, &QPushButton::clicked, this, [this] {
        if (canvas_->duplicateActiveLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("The layer could not be duplicated"),
                                     3000);
        }
    });
    connect(mergeButton, &QPushButton::clicked, this, [this] {
        if (canvas_->mergeActiveLayerDown()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(
                tr("Select a visible, unlocked upper layer above another visible, unlocked layer"),
                4000);
        }
    });
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        if (canvas_->deleteActiveLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("Background cannot be deleted"), 3000);
        }
    });
    connect(upButton, &QPushButton::clicked, this, [this] {
        if (canvas_->moveActiveLayer(1)) {
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(downButton, &QPushButton::clicked, this, [this] {
        if (canvas_->moveActiveLayer(-1)) {
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(renameButton, &QPushButton::clicked, this, [this] {
        const int index = canvas_->activeLayerIndex();
        if (index <= 0) {
            statusBar()->showMessage(tr("Background cannot be renamed"), 3000);
            return;
        }
        bool accepted = false;
        const QString current = QString::fromStdString(
            activeLevel()->layers[static_cast<std::size_t>(index)].name);
        const QString name = QInputDialog::getText(
            this, tr("Rename layer"), tr("Layer name:"), QLineEdit::Normal,
            current, &accepted);
        if (accepted && !name.trimmed().isEmpty() && name != current) {
            canvas_->renameLayer(index, name.trimmed());
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(lockButton, &QPushButton::clicked, this, [this] {
        const int index = canvas_->activeLayerIndex();
        if (index < 0) {
            return;
        }
        const bool locked = activeLevel()
                                ->layers[static_cast<std::size_t>(index)]
                                .locked;
        canvas_->setLayerLocked(index, !locked);
        nonUndoModified_ = true;
        setModified(true);
    });
    refreshLayerList();
}

void MainWindow::refreshLayerList()
{
    Level* document = activeLevel();
    if (layerListWidget_ == nullptr || document == nullptr) {
        return;
    }
    const QSignalBlocker blocker(layerListWidget_);
    layerListWidget_->clear();
    for (std::size_t index = document->layers.size(); index-- > 0;) {
        const Level::Layer& layer = document->layers[index];
        QString label = QString::fromStdString(layer.name);
        if (layer.locked) {
            label += tr("  [locked]");
        }
        auto* item = new QListWidgetItem(label, layerListWidget_);
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        if (index == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setToolTip(tr("Background is always visible and always bottom"));
        }
        if (index == document->activeLayer) {
            layerListWidget_->setCurrentItem(item);
        }
    }
}

void MainWindow::createObjectDock()
{
    objectDock_ = new QDockWidget(tr("Objects"), this);
    auto* contents = new QWidget(objectDock_);
    auto* layout = new QVBoxLayout(contents);
    objectModeCheck_ = new QCheckBox(tr("Edit objects on canvas"), contents);
    objectModeCheck_->setChecked(false);
    layout->addWidget(objectModeCheck_);
    auto* tools = new QHBoxLayout();
    const auto addObjectTool = [&](const QString& icon, const QString& label,
                                   const char* type) {
        auto* button = new QToolButton(contents);
        button->setIcon(QIcon(icon));
        button->setIconSize(QSize(32, 32));
        button->setToolTip(label);
        button->setAccessibleName(label);
        connect(button, &QToolButton::clicked, this,
                [this, type] { addOverThrustObject(type); });
        tools->addWidget(button);
    };
    addObjectTool(QStringLiteral(":/icons/overthrust/tool-door.png"),
                  tr("Create sliding gate"), "sliding_door");
    addObjectTool(QStringLiteral(":/icons/overthrust/tool-teleporter.png"),
                  tr("Create teleporter"), "teleporter");
    addObjectTool(QStringLiteral(":/icons/overthrust/tool-service-orb.png"),
                  tr("Create service orb"), "service_orb");
    addObjectTool(QStringLiteral(":/icons/overthrust/tool-flag.png"),
                  tr("Create capture flag"), "capture_flag");
    tools->addStretch();
    layout->addLayout(tools);
    auto* markTeamBase = new QPushButton(
        QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_polygon.png")),
        tr("Mark team-base area..."), contents);
    markTeamBase->setToolTip(
        tr("Draw a polygon that defines one team's base area."));
    layout->addWidget(markTeamBase);
    objectListWidget_ = new QListWidget(contents);
    objectListWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(objectListWidget_, 1);
    auto* row = new QHBoxLayout();
    auto* add = new QPushButton(tr("Create"), contents);
    auto* edit = new QPushButton(tr("Properties"), contents);
    auto* duplicate = new QPushButton(tr("Duplicate"), contents);
    auto* remove = new QPushButton(tr("Delete"), contents);
    auto* redraw = new QPushButton(tr("Redraw area"), contents);
    row->addWidget(add);
    row->addWidget(edit);
    row->addWidget(duplicate);
    row->addWidget(remove);
    layout->addLayout(row);
    layout->addWidget(redraw);
    auto* hint = new QLabel(
        tr("Drag an object to move it. Selected objects have separate width, height, corner-resize, and rotation handles. Properties remains available for precise values and teleporter connections."),
        contents);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    objectDock_->setWidget(contents);
    addDockWidget(Qt::RightDockWidgetArea, objectDock_);
    objectDock_->setVisible(false);
    connect(add, &QPushButton::clicked, this,
            [this] { addOverThrustObject(); });
    connect(markTeamBase, &QPushButton::clicked, this,
            [this] { addOverThrustObject("team_base"); });
    connect(edit, &QPushButton::clicked, this,
            &MainWindow::editSelectedOverThrustObject);
    connect(duplicate, &QPushButton::clicked, this,
            &MainWindow::duplicateSelectedOverThrustObject);
    connect(remove, &QPushButton::clicked, this,
            &MainWindow::deleteSelectedOverThrustObject);
    connect(redraw, &QPushButton::clicked, this,
            &MainWindow::redrawSelectedTeamBase);
    connect(objectListWidget_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { editSelectedOverThrustObject(); });
    connect(objectListWidget_, &QListWidget::currentRowChanged, canvas_,
            &LevelCanvas::setSelectedObject);
    connect(objectModeCheck_, &QCheckBox::toggled, this,
            [this](const bool enabled) {
                canvas_->setObjectEditingEnabled(
                    enabled && creationSettings_.game == GameId::OverThrust);
            });
    refreshObjectList();
}

void MainWindow::refreshObjectList()
{
    const bool enabled = creationSettings_.game == GameId::OverThrust &&
                         overthrustData_ != nullptr &&
                         documentTabs_->currentIndex() == 0;
    objectDock_->setVisible(enabled);
    canvas_->setObjectEditingEnabled(enabled && objectModeCheck_->isChecked());
    canvas_->setObjects(enabled ? &overthrustData_->objects : nullptr);
    if (!enabled) return;
    for (const OverThrustObject& object : overthrustData_->objects) {
        if (object.type != "sliding_door" ||
            object.activationMode != "channel" ||
            object.activationChannel.empty()) {
            continue;
        }
        const auto found = std::find_if(
            overthrustData_->channels.begin(), overthrustData_->channels.end(),
            [&](const OverThrustChannel& channel) {
                return channel.id == object.activationChannel;
            });
        if (found == overthrustData_->channels.end()) {
            overthrustData_->channels.push_back(
                {object.activationChannel, false});
        }
    }
    const int selected = objectListWidget_->currentRow();
    const QSignalBlocker blocker(objectListWidget_);
    objectListWidget_->clear();
    for (const OverThrustObject& object : overthrustData_->objects) {
        QString iconPath;
        if (object.type == "sliding_door")
            iconPath = QStringLiteral(":/icons/overthrust/tool-door.png");
        else if (object.type == "teleporter")
            iconPath = QStringLiteral(":/icons/overthrust/tool-teleporter.png");
        else if (object.type == "service_orb")
            iconPath = QStringLiteral(":/icons/overthrust/tool-service-orb.png");
        else if (object.type == "team_base")
            iconPath = QStringLiteral(":/icons/icons/kolourpaint/tool_polygon.png");
        else if (object.type == "capture_flag")
            iconPath = QStringLiteral(":/icons/overthrust/tool-flag.png");
        const QString typeLabel =
            object.type == "sliding_door"
                ? tr("sliding gate")
                : QString::fromStdString(object.type);
        auto* item = new QListWidgetItem(
            QIcon(iconPath),
            QStringLiteral("%1  [%2]")
                .arg(QString::fromStdString(object.id),
                     typeLabel),
            objectListWidget_);
        item->setToolTip(QString::fromStdString(object.id));
    }
    const int restoredSelection =
        selected >= 0 && !overthrustData_->objects.empty()
            ? std::min(selected,
                       static_cast<int>(overthrustData_->objects.size()) - 1)
            : -1;
    objectListWidget_->setCurrentRow(restoredSelection);
    canvas_->setSelectedObject(restoredSelection);
    canvas_->refreshImage();
}

void MainWindow::applyObjectListChange(
    const QString& text, std::vector<OverThrustObject> before,
    std::vector<OverThrustObject> after)
{
    if (!overthrustData_) return;
    canvas_->undoStack()->push(new ObjectListCommand(
        &overthrustData_->objects, std::move(before), std::move(after),
        [this] { refreshObjectList(); }, text));
    setModified(true);
}

bool MainWindow::editOverThrustObject(OverThrustObject& object,
                                      const bool creating)
{
    const bool knownType = object.type == "sliding_door" ||
                           object.type == "teleporter" ||
                           object.type == "service_orb" ||
                           object.type == "team_base" ||
                           object.type == "capture_flag";
    if (!knownType) {
        QMessageBox::information(
            this, tr("Unknown object type"),
            tr("This object is retained for forward compatibility but cannot be edited or published by this profile revision."));
        return false;
    }
    const std::string originalId = object.id;
    QDialog dialog(this);
    dialog.setWindowTitle(creating ? tr("Create OverThrust object")
                                   : tr("Object properties"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    form->addRow(tr("Type:"),
                 new QLabel(QString::fromStdString(object.type), &dialog));
    auto* id = new QLineEdit(QString::fromStdString(object.id), &dialog);
    id->setMaxLength(256);
    form->addRow(tr("Stable ID:"), id);
    const auto integer = [&dialog](const int minimum, const int maximum,
                                   const int value) {
        auto* spin = new QSpinBox(&dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        return spin;
    };
    auto* x = integer(0, static_cast<int>(level_->width) - 1,
                      object.transform.x);
    auto* y = integer(0, static_cast<int>(level_->height) - 1,
                      object.transform.y);
    auto* width = integer(1, static_cast<int>(level_->width),
                          object.transform.width);
    auto* height = integer(1, static_cast<int>(level_->height),
                           object.transform.height);
    auto* rotation = new QDoubleSpinBox(&dialog);
    rotation->setRange(-359.999, 359.999);
    rotation->setDecimals(3);
    rotation->setSuffix(QStringLiteral("°"));
    rotation->setValue(object.transform.rotationMillidegrees / 1000.0);
    form->addRow(tr("X (px):"), x);
    form->addRow(tr("Y (px):"), y);
    form->addRow(tr("Width (px):"), width);
    form->addRow(tr("Height (px):"), height);
    form->addRow(tr("Rotation:"), rotation);
    if (object.type == "team_base") {
        width->setEnabled(false);
        height->setEnabled(false);
        rotation->setEnabled(false);
        width->setToolTip(tr("Team-base size is defined by its polygon."));
        height->setToolTip(tr("Team-base size is defined by its polygon."));
    }

    auto* appearance = new QComboBox(&dialog);
    auto addAppearances = [&](const std::initializer_list<const char*> values) {
        for (const char* value : values) {
            appearance->addItem(QString::fromLatin1(value));
        }
        const int current = appearance->findText(
            QString::fromStdString(object.appearanceId));
        if (current >= 0) appearance->setCurrentIndex(current);
    };
    QSpinBox* slideDistance = nullptr;
    QDoubleSpinBox* speed = nullptr;
    QComboBox* initialState = nullptr;
    QComboBox* activationMode = nullptr;
    QDoubleSpinBox* activationRadius = nullptr;
    QLineEdit* channel = nullptr;
    QComboBox* material = nullptr;
    QDoubleSpinBox* radius = nullptr;
    QComboBox* target = nullptr;
    QComboBox* velocity = nullptr;
    QComboBox* serviceProfile = nullptr;
    QSpinBox* teamSlot = nullptr;
    QComboBox* accessMode = nullptr;
    QLineEdit* accessSlots = nullptr;
    if (object.type == "sliding_door") {
        const int currentDistance = std::max(
            1, static_cast<int>(std::round(std::hypot(
                   object.openOffsetX, object.openOffsetY))));
        slideDistance = integer(1, 4096, currentDistance);
        speed = new QDoubleSpinBox(&dialog);
        speed->setRange(0.001, 1024);
        speed->setDecimals(6);
        speed->setValue(object.speedPixelsPerSecond);
        initialState = new QComboBox(&dialog);
        initialState->addItems({QStringLiteral("closed"), QStringLiteral("open")});
        initialState->setCurrentText(QString::fromStdString(object.initialState));
        activationMode = new QComboBox(&dialog);
        activationMode->addItems({QStringLiteral("proximity"),
                                  QStringLiteral("channel")});
        activationMode->setCurrentText(
            QString::fromStdString(object.activationMode));
        activationRadius = new QDoubleSpinBox(&dialog);
        activationRadius->setRange(0.001, 1024);
        activationRadius->setValue(object.activationRadiusPixels);
        channel = new QLineEdit(QString::fromStdString(object.activationChannel),
                                &dialog);
        material = new QComboBox(&dialog);
        material->addItems({QStringLiteral("core:aluminium"),
                            QStringLiteral("core:titanium"),
                            QStringLiteral("core:tungsten")});
        material->setCurrentText(QString::fromStdString(object.materialId));
        addAppearances({"appearance:door/aluminium",
                        "appearance:door/titanium",
                        "appearance:door/tungsten"});
        form->addRow(tr("Slide distance along arrow (px):"), slideDistance);
        form->addRow(tr("Speed (px/s):"), speed);
        form->addRow(tr("Initial state:"), initialState);
        form->addRow(tr("Activation:"), activationMode);
        form->addRow(tr("Proximity radius (px):"), activationRadius);
        form->addRow(tr("Activation channel:"), channel);
        form->addRow(tr("Gate material:"), material);
    } else if (object.type == "teleporter") {
        radius = new QDoubleSpinBox(&dialog);
        radius->setRange(0.001, 1024);
        radius->setValue(object.radiusPixels);
        target = new QComboBox(&dialog);
        target->setEditable(true);
        target->addItem(QString());
        for (const OverThrustObject& candidate : overthrustData_->objects) {
            if (candidate.type == "teleporter" &&
                candidate.id != originalId) {
                target->addItem(QString::fromStdString(candidate.id));
            }
        }
        const QString currentTarget =
            QString::fromStdString(object.targetObjectId);
        if (!currentTarget.isEmpty() && target->findText(currentTarget) < 0)
            target->addItem(currentTarget);
        target->setCurrentText(currentTarget);
        velocity = new QComboBox(&dialog);
        velocity->addItems({QStringLiteral("preserve"), QStringLiteral("zero"),
                            QStringLiteral("rotate_to_exit")});
        velocity->setCurrentText(QString::fromStdString(object.velocityMode));
        addAppearances({"appearance:teleporter/blue",
                        "appearance:teleporter/orange"});
        form->addRow(tr("Interaction radius (px):"), radius);
        form->addRow(tr("Target teleporter ID:"), target);
        form->addRow(tr("Velocity mode:"), velocity);
    } else if (object.type == "service_orb") {
        radius = new QDoubleSpinBox(&dialog);
        radius->setRange(0.001, 1024);
        radius->setValue(object.radiusPixels);
        serviceProfile = new QComboBox(&dialog);
        serviceProfile->addItems({QStringLiteral("core:heal-only"),
                                  QStringLiteral("core:heal-and-weapons")});
        serviceProfile->setCurrentText(
            QString::fromStdString(object.serviceProfileId));
        addAppearances({"appearance:service-orb/default",
                        "appearance:service-orb/private"});
        form->addRow(tr("Interaction radius (px):"), radius);
        form->addRow(tr("Service profile:"), serviceProfile);
    } else if (object.type == "team_base") {
        teamSlot = integer(1, 4, object.teamSlot);
        addAppearances({"appearance:team-base/default"});
        form->addRow(tr("Team slot:"), teamSlot);
    } else {
        teamSlot = integer(1, 4, object.teamSlot);
        form->addRow(tr("Team slot:"), teamSlot);
    }
    if (object.type != "team_base" && object.type != "capture_flag") {
        accessMode = new QComboBox(&dialog);
        accessMode->addItems({QStringLiteral("everyone"),
                              QStringLiteral("player_slots")});
        accessMode->setCurrentIndex(object.access.everyone ? 0 : 1);
        QStringList slotTexts;
        for (const int slot : object.access.playerSlots) {
            slotTexts.push_back(QString::number(slot));
        }
        accessSlots =
            new QLineEdit(slotTexts.join(QStringLiteral(",")), &dialog);
        accessSlots->setPlaceholderText(tr("For example: 1,3"));
        form->addRow(tr("Access:"), accessMode);
        form->addRow(tr("Player slots:"), accessSlots);
    }
    if (object.type != "capture_flag") {
        form->addRow(tr("Appearance:"), appearance);
    }
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    const QString editedId = id->text().trimmed();
    const QRegularExpression idPattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9:._/-]{0,255}$"));
    const bool duplicateId = overthrustData_ && std::any_of(
        overthrustData_->objects.begin(), overthrustData_->objects.end(),
        [&](const OverThrustObject& existing) {
            return existing.id == editedId.toStdString() &&
                   (creating || existing.id != originalId);
        });
    if (!idPattern.match(editedId).hasMatch() || duplicateId) {
        QMessageBox::critical(
            this, tr("Invalid object ID"),
            duplicateId ? tr("Every object ID must be unique.")
                        : tr("Use 1–256 identifier characters beginning with a letter or number."));
        return false;
    }
    object.id = editedId.toStdString();
    const int moveX = x->value() - object.transform.x;
    const int moveY = y->value() - object.transform.y;
    if (object.type == "team_base") {
        for (OverThrustPoint& point : object.areaPolygon) {
            point.x += moveX;
            point.y += moveY;
        }
    }
    object.transform = {x->value(), y->value(), width->value(),
                        height->value(),
                        static_cast<int>(std::round(rotation->value() * 1000))};
    if (object.type != "capture_flag") {
        object.appearanceId = appearance->currentText().toStdString();
    }
    if (slideDistance) {
        const double radians = object.transform.rotationMillidegrees *
            3.14159265358979323846 / 180000.0;
        object.openOffsetX = static_cast<int>(std::round(
            slideDistance->value() * std::cos(radians)));
        object.openOffsetY = static_cast<int>(std::round(
            slideDistance->value() * std::sin(radians)));
        object.speedPixelsPerSecond = speed->value();
        object.initialState = initialState->currentText().toStdString();
        object.activationMode = activationMode->currentText().toStdString();
        object.activationRadiusPixels = activationRadius->value();
        object.activationChannel = channel->text().toStdString();
        object.materialId = material->currentText().toStdString();
    }
    if (radius) object.radiusPixels = radius->value();
    if (target)
        object.targetObjectId = target->currentText().trimmed().toStdString();
    if (velocity) object.velocityMode = velocity->currentText().toStdString();
    if (serviceProfile) {
        object.serviceProfileId = serviceProfile->currentText().toStdString();
        object.services = object.serviceProfileId == "core:heal-only"
                              ? std::vector<std::string>{"heal"}
                              : std::vector<std::string>{"heal",
                                                         "weapon_change"};
    }
    if (teamSlot) {
        object.teamSlot = teamSlot->value();
        if (object.type == "team_base") {
            object.baseProfileId = "core:team-base/heal-and-weapons";
        }
    }
    if (accessMode) {
        object.access.everyone = accessMode->currentIndex() == 0;
        object.access.playerSlots.clear();
        if (!object.access.everyone) {
            for (const QString& slot : accessSlots->text().split(
                     QLatin1Char(','), Qt::SkipEmptyParts)) {
                object.access.playerSlots.push_back(slot.trimmed().toInt());
            }
        }
    }
    return true;
}

void MainWindow::addOverThrustObject()
{
    if (!overthrustData_) return;
    const QStringList types{tr("Sliding gate"), tr("Teleporter"),
                            tr("Service orb"), tr("Capture flag"),
                            tr("Team base")};
    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this, tr("Create object"), tr("Object type:"), types, 0, false,
        &accepted);
    if (!accepted) return;
    const int type = types.indexOf(selected);
    addOverThrustObject(type == 0   ? "sliding_door"
                        : type == 1 ? "teleporter"
                        : type == 2 ? "service_orb"
                        : type == 3 ? "capture_flag"
                                    : "team_base");
}

void MainWindow::addOverThrustObject(const std::string& type)
{
    if (!overthrustData_) return;
    if (type == "team_base") {
        objectModeCheck_->setChecked(true);
        canvas_->beginTeamBasePolygon();
        statusBar()->showMessage(
            tr("Click polygon corners; double-click, right-click, or press Enter to finish. Escape cancels."));
        return;
    }
    OverThrustObject object;
    object.type = type;
    std::size_t sequence = overthrustData_->objects.size() + 1;
    do {
        object.id = object.type + "_" + std::to_string(sequence++);
    } while (std::any_of(
        overthrustData_->objects.begin(), overthrustData_->objects.end(),
        [&](const OverThrustObject& existing) {
            return existing.id == object.id;
        }));
    const int objectWidth = object.type == "sliding_door" ? 12 : 8;
    const int objectHeight = object.type == "sliding_door" ? 4 : 8;
    object.transform = {
        std::max(0, (static_cast<int>(level_->width) - objectWidth) / 2),
        std::max(0, (static_cast<int>(level_->height) - objectHeight) / 2),
        objectWidth, objectHeight, 0};
    if (object.type == "sliding_door") {
        object.openOffsetX = objectWidth;
        object.materialId = "core:aluminium";
        object.appearanceId = "appearance:door/aluminium";
    } else if (object.type == "teleporter") {
        object.appearanceId = "appearance:teleporter/blue";
    } else if (object.type == "service_orb") {
        object.serviceProfileId = "core:heal-only";
        object.services = {"heal"};
        object.appearanceId = "appearance:service-orb/default";
    } else if (object.type == "capture_flag") {
        object.teamSlot = 1;
    }
    auto before = overthrustData_->objects;
    auto after = before;
    if (object.type == "teleporter" && object.targetObjectId.empty()) {
        const auto unpaired = std::find_if(
            after.begin(), after.end(), [](const OverThrustObject& existing) {
                return existing.type == "teleporter" &&
                       existing.targetObjectId.empty();
            });
        if (unpaired != after.end()) {
            object.targetObjectId = unpaired->id;
            unpaired->targetObjectId = object.id;
        }
    }
    after.push_back(std::move(object));
    applyObjectListChange(tr("Create object"), std::move(before),
                          std::move(after));
    objectListWidget_->setCurrentRow(
        static_cast<int>(overthrustData_->objects.size()) - 1);
    statusBar()->showMessage(
        tr("Object created at the level center. Drag it or use its transform handles; open Properties for gameplay settings."),
        5000);
}

void MainWindow::redrawSelectedTeamBase()
{
    if (!overthrustData_) return;
    const int row = objectListWidget_->currentRow();
    if (row < 0 || row >= static_cast<int>(overthrustData_->objects.size()) ||
        overthrustData_->objects[static_cast<std::size_t>(row)].type !=
            "team_base") {
        QMessageBox::information(this, tr("Redraw team-base area"),
                                 tr("Select a team-base object first."));
        return;
    }
    objectModeCheck_->setChecked(true);
    canvas_->beginTeamBasePolygon(row);
    statusBar()->showMessage(
        tr("Click replacement polygon corners; double-click, right-click, or press Enter to finish. Escape cancels."));
}

void MainWindow::applyTeamBasePolygon(const QPolygon& polygon,
                                      const int objectIndex)
{
    if (!overthrustData_ || polygon.size() < 3) return;
    const QRect bounds = polygon.boundingRect();
    if (bounds.width() < 2 || bounds.height() < 2) {
        QMessageBox::warning(this, tr("Invalid team-base area"),
                             tr("The polygon must enclose a non-zero area."));
        return;
    }
    const auto assignPolygon = [&](OverThrustObject& object) {
        object.areaPolygon.clear();
        object.areaPolygon.reserve(static_cast<std::size_t>(polygon.size()));
        for (const QPoint& point : polygon)
            object.areaPolygon.push_back({point.x(), point.y()});
        object.transform = {bounds.left(), bounds.top(), bounds.width(),
                            bounds.height(), 0};
    };

    auto before = overthrustData_->objects;
    auto after = before;
    if (objectIndex >= 0 && objectIndex < static_cast<int>(after.size()) &&
        after[static_cast<std::size_t>(objectIndex)].type == "team_base") {
        assignPolygon(after[static_cast<std::size_t>(objectIndex)]);
        applyObjectListChange(tr("Redraw team-base area"), std::move(before),
                              std::move(after));
        objectListWidget_->setCurrentRow(objectIndex);
        return;
    }

    OverThrustObject object;
    object.type = "team_base";
    std::size_t sequence = after.size() + 1;
    do {
        object.id = object.type + "_" + std::to_string(sequence++);
    } while (std::any_of(after.begin(), after.end(),
                         [&](const OverThrustObject& existing) {
                             return existing.id == object.id;
                         }));
    object.baseProfileId = "core:team-base/heal-and-weapons";
    object.appearanceId = "appearance:team-base/default";
    assignPolygon(object);
    if (!editOverThrustObject(object, true)) return;
    after.push_back(std::move(object));
    applyObjectListChange(tr("Create team base"), std::move(before),
                          std::move(after));
    objectListWidget_->setCurrentRow(-1);
    objectListWidget_->clearSelection();
    canvas_->setSelectedObject(-1);
}

void MainWindow::editSelectedOverThrustObject()
{
    if (!overthrustData_) return;
    const int row = objectListWidget_->currentRow();
    if (row < 0 || row >= static_cast<int>(overthrustData_->objects.size()))
        return;
    auto before = overthrustData_->objects;
    auto after = before;
    if (!editOverThrustObject(after[static_cast<std::size_t>(row)], false))
        return;
    applyObjectListChange(tr("Edit object properties"), std::move(before),
                          std::move(after));
    objectListWidget_->setCurrentRow(row);
}

void MainWindow::duplicateSelectedOverThrustObject()
{
    if (!overthrustData_) return;
    const int row = objectListWidget_->currentRow();
    if (row < 0 || row >= static_cast<int>(overthrustData_->objects.size()))
        return;
    auto before = overthrustData_->objects;
    auto after = before;
    OverThrustObject copy = after[static_cast<std::size_t>(row)];
    const std::string baseId = copy.id + "_copy";
    copy.id = baseId;
    int suffix = 2;
    const auto idExists = [&](const std::string& id) {
        return std::any_of(after.begin(), after.end(),
                           [&](const OverThrustObject& existing) {
                               return existing.id == id;
                           });
    };
    while (idExists(copy.id)) {
        copy.id = baseId + std::to_string(suffix++);
    }
    copy.transform.x = std::min(copy.transform.x + 1,
                                static_cast<int>(level_->width) -
                                    copy.transform.width);
    copy.transform.y = std::min(copy.transform.y + 1,
                                static_cast<int>(level_->height) -
                                    copy.transform.height);
    after.push_back(std::move(copy));
    applyObjectListChange(tr("Duplicate object"), std::move(before),
                          std::move(after));
    objectListWidget_->setCurrentRow(
        static_cast<int>(overthrustData_->objects.size()) - 1);
}

void MainWindow::deleteSelectedOverThrustObject()
{
    if (!overthrustData_) return;
    const int row = objectListWidget_->currentRow();
    if (row < 0 || row >= static_cast<int>(overthrustData_->objects.size()))
        return;
    auto before = overthrustData_->objects;
    auto after = before;
    after.erase(after.begin() + row);
    applyObjectListChange(tr("Delete object"), std::move(before),
                          std::move(after));
}

void MainWindow::openLevel()
{
    if (!maybeSave()) {
        return;
    }

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open project or level"), QString(),
        tr("PCX Level Tool projects, levels and PCX (*.pxlp *.PXLP *.lev *.LEV *.pcx *.PCX);;"
           "PCX Level Tool projects (*.pxlp *.PXLP);;"
           "Supported game levels (*.lev *.LEV);;"
           "Indexed PCX images (*.pcx *.PCX)"));
    if (filename.isEmpty()) {
        return;
    }

    auto loaded = std::make_unique<Level>();
    std::string error;
    const std::filesystem::path path = toPath(filename);
    const QString suffix = QFileInfo(filename).suffix();
    const bool isPxlProject =
        QString::compare(suffix, QStringLiteral("pxlp"),
                         Qt::CaseInsensitive) == 0;
    const bool isPcx =
        QString::compare(suffix, QStringLiteral("pcx"),
                         Qt::CaseInsensitive) == 0;
    const bool isProject = isPxlProject;
    std::unique_ptr<Level> loadedBackground;
    std::unique_ptr<OverThrustProjectData> loadedOverThrust;
    LevelCreationSettings loadedSettings;
    bool loadedSuccessfully = false;
    if (isPxlProject) {
        loadedSuccessfully = loadPxlProject(
            path, *loaded, loadedBackground, loadedSettings, loadedOverThrust,
            error);
    } else if (isPcx) {
        loadedSuccessfully = loadPcx(path, *loaded, error);
        if (loadedSuccessfully) {
            QStringList compatibleNames;
            std::vector<GameId> compatibleGames;
            for (const GameProfile& profile : availableGameProfiles()) {
                if (dimensionsFitProfile(profile, loaded->width,
                                         loaded->height)) {
                    compatibleNames.push_back(QString::fromUtf8(
                        profile.displayName.data(), profile.displayName.size()));
                    compatibleGames.push_back(profile.id);
                }
            }
            if (compatibleGames.empty()) {
                loadedSuccessfully = false;
                error = "PCX dimensions are not supported by any game profile";
            } else {
                bool selected = false;
                const QString gameName = QInputDialog::getItem(
                    this, tr("Open PCX image"), tr("Use game profile:"),
                    compatibleNames, 0, false, &selected);
                if (!selected) {
                    return;
                }
                const int selectedIndex = compatibleNames.indexOf(gameName);
                if (selectedIndex < 0) {
                    return;
                }
                loadedSettings.game = compatibleGames[static_cast<std::size_t>(
                    selectedIndex)];
                loadedSettings.name = loaded->name;
                loadedSettings.width = static_cast<int>(loaded->width);
                loadedSettings.height = static_cast<int>(loaded->height);
                if (loadedSettings.game == GameId::OverThrust) {
                    loadedSettings.backgroundMode = BackgroundMode::None;
                    loadedOverThrust =
                        std::make_unique<OverThrustProjectData>();
                    loadedOverThrust->levelId = loaded->name;
                    loadedOverThrust->displayName = loaded->name;
                }
                if (loadedSettings.game == GameId::Auts) {
                    applyAutsGameRules(*loaded);
                }
            }
        }
    } else {
        std::string vwingError;
        loadedSuccessfully = loadLev(path, *loaded, vwingError);
        if (!loadedSuccessfully) {
            std::string autsError;
            loadedSuccessfully = loadAutsLev(path, *loaded, autsError);
            if (loadedSuccessfully) {
                loadedSettings.game = GameId::Auts;
            } else {
                error = "not a supported V-Wing or AUTS LEV file\nV-Wing: " +
                        vwingError + "\nAUTS: " + autsError;
            }
        }
    }
    if (!loadedSuccessfully) {
        QMessageBox::critical(this, tr("Open failed"),
                              QString::fromStdString(error));
        return;
    }

    // Drop commands while their old Level target is still alive.
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(loaded);
    backgroundLevel_ = std::move(loadedBackground);
    overthrustData_ = std::move(loadedOverThrust);
    creationSettings_ = loadedSettings;
    if (!isPxlProject && !isPcx) {
        if (creationSettings_.game != GameId::Auts) {
            creationSettings_.game = GameId::VWing;
        }
        creationSettings_.name = level_->name;
        creationSettings_.width = static_cast<int>(level_->width);
        creationSettings_.height = static_cast<int>(level_->height);
    }
    levelSettingsAction_->setEnabled(creationSettings_.game == GameId::Wings ||
                                     creationSettings_.game ==
                                         GameId::OverThrust);
    levelSettingsAction_->setText(
        creationSettings_.game == GameId::OverThrust
            ? tr("OverThrust level &settings...")
            : tr("Wings level &settings..."));
    documentTabs_->setCurrentIndex(0);
    configurePaletteForGame();
    configureProfileDocuments();
    projectPath_ = isProject ? path : std::filesystem::path{};
    publishPath_ = isProject || isPcx ? std::filesystem::path{} : path;
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(
            QString::fromLatin1(level_->name.data(),
                                static_cast<int>(level_->name.size())));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Opened %1").arg(filename), 3000);
}

void MainWindow::exportPcx()
{
    Level* document = activeLevel();
    if (document == nullptr) {
        return;
    }
    QString suggested = QString::fromStdString(document->name);
    if (suggested.isEmpty()) {
        suggested = tr("Untitled");
    }
    suggested += QStringLiteral(".pcx");
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Export indexed PCX image"), suggested,
        tr("Indexed PCX images (*.pcx *.PCX)"));
    if (filename.isEmpty()) {
        return;
    }
    if (!filename.endsWith(QStringLiteral(".pcx"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".pcx");
    }
    canvas_->commitSelection();
    flattenLayers(*document);
    std::string error;
    if (!savePcx(toPath(filename), *document, error)) {
        QMessageBox::critical(this, tr("PCX export failed"),
                              QString::fromStdString(error));
        return;
    }
    statusBar()->showMessage(tr("Exported %1").arg(filename), 3000);
}

void MainWindow::importImage()
{
    QStringList gameNames;
    for (const GameProfile& profile : availableGameProfiles()) {
        gameNames.push_back(QString::fromUtf8(profile.displayName.data(),
                                              profile.displayName.size()));
    }
    bool selected = false;
    const QString gameName = QInputDialog::getItem(
        this, tr("Import image"), tr("Target game:"), gameNames, 0, false,
        &selected);
    if (!selected) {
        return;
    }
    const int selectedIndex = gameNames.indexOf(gameName);
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(availableGameProfiles().size())) {
        return;
    }
    const GameProfile& profile =
        availableGameProfiles()[static_cast<std::size_t>(selectedIndex)];

    const bool auts = profile.id == GameId::Auts;
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Import image for %1").arg(gameName), QString(),
        auts ? tr("Indexed images (*.pcx *.PCX *.bmp *.BMP)")
             : tr("8-bit indexed PCX images (*.pcx *.PCX)"));
    if (filename.isEmpty()) {
        return;
    }

    auto imported = std::make_unique<Level>();
    bool paletteMatches = true;
    std::string error;
    const bool bmp = QFileInfo(filename).suffix().compare(
                         QStringLiteral("bmp"), Qt::CaseInsensitive) == 0;
    const bool loaded =
        bmp ? auts && loadAutsBmp(toPath(filename), *imported, paletteMatches,
                                  error)
            : loadPcx(toPath(filename), *imported, error);
    if (!loaded) {
        if (bmp && !auts) {
            error = "BMP import is currently supported only for AUTS";
        }
        QMessageBox::critical(this, tr("Image import failed"),
                              QString::fromStdString(error));
        return;
    }

    if (!dimensionsFitProfile(profile, imported->width, imported->height)) {
        QMessageBox::critical(
            this, tr("Wrong image dimensions"),
            tr("%1 requires an image between %2 x %3 and %4 x %5 pixels. "
               "The selected image is %6 x %7.")
                .arg(gameName)
                .arg(profile.minimumWidth)
                .arg(profile.minimumHeight)
                .arg(profile.maximumWidth)
                .arg(profile.maximumHeight)
                .arg(imported->width)
                .arg(imported->height));
        return;
    }

    LevelCreationSettings settings;
    settings.game = profile.id;
    settings.name = imported->name;
    settings.width = static_cast<int>(imported->width);
    settings.height = static_cast<int>(imported->height);
    bool paletteAdjusted = false;
    if (profile.id == GameId::Auts) {
        paletteAdjusted = !paletteMatches ||
                          imported->palette != defaultAutsPalette();
        applyAutsGameRules(*imported);
    } else if (profile.id == GameId::Wings) {
        const auto fixed = defaultWingsPalette();
        for (std::size_t index = 0; index < imported->palette.size(); ++index) {
            if (index < 48 ||
                isReservedPaletteIndex(GameId::Wings,
                                       static_cast<int>(index))) {
                paletteAdjusted = paletteAdjusted ||
                                  !(imported->palette[index] == fixed[index]);
                imported->palette[index] = fixed[index];
            }
        }
        if (!promptForWingsSettings(settings, false)) {
            return;
        }
        imported->name = settings.name;
    }
    if (!maybeSave()) {
        return;
    }

    installImportedLevel(
        std::move(imported), settings,
        tr("Imported %1 for %2").arg(filename, gameName));
    if (profile.id == GameId::Auts || profile.id == GameId::VWing) {
        uppercaseLevelName();
    }
    if (paletteAdjusted) {
        QMessageBox::information(
            this, tr("Game palette rules applied"),
            profile.id == GameId::Auts
                ? tr("Pixel indices were preserved and the fixed AUTS palette "
                     "and boundary were applied.")
                : tr("Pixel indices were preserved. Wings' fixed and reserved "
                     "palette entries were restored to their game values."));
    }
}

void MainWindow::installImportedLevel(
    std::unique_ptr<Level> imported, const LevelCreationSettings& settings,
    const QString& statusMessage)
{
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(imported);
    backgroundLevel_.reset();
    overthrustData_.reset();
    creationSettings_ = settings;
    if (settings.game == GameId::OverThrust) {
        overthrustData_ = std::make_unique<OverThrustProjectData>();
        overthrustData_->levelId = level_->name;
        overthrustData_->displayName = level_->name;
        creationSettings_.backgroundMode = BackgroundMode::None;
    }
    projectPath_.clear();
    publishPath_.clear();
    documentTabs_->setCurrentIndex(0);
    configurePaletteForGame();
    configureProfileDocuments();
    levelSettingsAction_->setEnabled(settings.game == GameId::Wings ||
                                     settings.game == GameId::OverThrust);
    levelSettingsAction_->setText(
        settings.game == GameId::OverThrust
            ? tr("OverThrust level &settings...")
            : tr("Wings level &settings..."));
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromStdString(level_->name));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = true;
    setModified(true);
    statusBar()->showMessage(statusMessage, 4000);
}

void MainWindow::newLevel()
{
    createNewLevel(true);
}

bool MainWindow::createNewLevel(const bool checkUnsavedChanges)
{
    if (checkUnsavedChanges && !maybeSave()) {
        return false;
    }

    QStringList gameNames;
    for (const GameProfile& profile : availableGameProfiles()) {
        gameNames.push_back(QString::fromUtf8(profile.displayName.data(),
                                              profile.displayName.size()));
    }
    bool selected = false;
    const QString gameName = QInputDialog::getItem(
        this, tr("New level"), tr("Game:"), gameNames, 0, false, &selected);
    if (!selected) {
        return false;
    }
    const int selectedIndex = gameNames.indexOf(gameName);
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(availableGameProfiles().size())) {
        return false;
    }

    LevelCreationSettings settings;
    std::unique_ptr<OverThrustProjectData> previousOverThrust;
    const GameProfile& profile =
        availableGameProfiles()[static_cast<std::size_t>(selectedIndex)];
    settings.game = profile.id;
    settings.width = profile.defaultWidth;
    settings.height = profile.defaultHeight;
    if (profile.id == GameId::Wings && !promptForWingsSettings(settings)) {
        return false;
    }
    if (profile.id == GameId::OverThrust) {
        previousOverThrust = std::move(overthrustData_);
        overthrustData_ = std::make_unique<OverThrustProjectData>();
        if (!promptForOverThrustSettings(settings)) {
            overthrustData_ = std::move(previousOverThrust);
            updateOverThrustBackgroundPreview();
            refreshObjectList();
            return false;
        }
    }

    auto fresh = std::make_unique<Level>();
    fresh->name = settings.name;
    if (settings.game == GameId::Wings) {
        fresh->palette = defaultWingsPalette();
    } else if (settings.game == GameId::OverThrust) {
        fresh->palette = defaultOverThrustPalette();
    } else if (settings.game == GameId::Auts) {
        initializeBlankAutsLevel(*fresh);
        fresh->name = settings.name;
    } else {
        fresh->palette = defaultVWingPalette();
    }
    if (settings.game == GameId::Wings ||
        settings.game == GameId::OverThrust) {
        fresh->resize(static_cast<std::size_t>(settings.width),
                      static_cast<std::size_t>(settings.height));
    } else if (settings.game == GameId::VWing) {
        fresh->pixels.fill(0);
    }

    // Drop commands while their old Level target is still alive.
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(fresh);
    if (settings.game != GameId::OverThrust) {
        overthrustData_.reset();
    }
    creationSettings_ = settings;
    documentTabs_->setCurrentIndex(0);
    configurePaletteForGame();
    configureProfileDocuments();
    levelSettingsAction_->setEnabled(settings.game == GameId::Wings ||
                                     settings.game == GameId::OverThrust);
    levelSettingsAction_->setText(
        settings.game == GameId::OverThrust
            ? tr("OverThrust level &settings...")
            : tr("Wings level &settings..."));
    projectPath_.clear();
    publishPath_.clear();
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromLatin1(
            level_->name.data(), static_cast<int>(level_->name.size())));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    if (settings.game == GameId::Wings) {
        const auto [backgroundWidth, backgroundHeight] =
            wingsParallaxSize(settings.width, settings.height);
        statusBar()->showMessage(
            settings.backgroundMode == BackgroundMode::Parallax
                ? tr("Created Wings level settings: %1 x %2, parallax %3 x %4")
                      .arg(settings.width)
                      .arg(settings.height)
                      .arg(backgroundWidth)
                      .arg(backgroundHeight)
                : tr("Created Wings level settings: %1 x %2")
                      .arg(settings.width)
                      .arg(settings.height),
            5000);
    } else if (settings.game == GameId::OverThrust) {
        statusBar()->showMessage(
            tr("Created a new OverThrust material grid: %1 x %2")
                .arg(settings.width)
                .arg(settings.height),
            4000);
    } else if (settings.game == GameId::Auts) {
        statusBar()->showMessage(
            tr("Created a new AUTS level: 320 x 400 with a protected border"),
            4000);
    } else {
        statusBar()->showMessage(tr("Created a new V-Wing level"), 3000);
    }
    updateWindowTitle();
    return true;
}

bool MainWindow::promptForWingsSettings(LevelCreationSettings& settings,
                                        const bool allowDimensionEditing)
{
    const GameProfile& profile = gameProfile(GameId::Wings);
    QDialog dialog(this);
    dialog.setWindowTitle(tr("New Wings level"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(QString::fromStdString(settings.name),
                                   &dialog);
    nameEdit->setMaxLength(64);
    nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9 _.-]{0,64}")),
        nameEdit));
    form->addRow(tr("Level / file name:"), nameEdit);

    auto* width = new QSpinBox(&dialog);
    width->setRange(profile.minimumWidth, profile.maximumWidth);
    width->setValue(settings.width);
    width->setSuffix(tr(" px"));
    width->setEnabled(allowDimensionEditing);
    form->addRow(tr("Width:"), width);

    auto* height = new QSpinBox(&dialog);
    height->setRange(profile.minimumHeight, profile.maximumHeight);
    height->setValue(settings.height);
    height->setSuffix(tr(" px"));
    height->setEnabled(allowDimensionEditing);
    form->addRow(tr("Height:"), height);

    auto* parallax = new QCheckBox(tr("Use parallax background"), &dialog);
    parallax->setChecked(settings.backgroundMode == BackgroundMode::Parallax);
    form->addRow(QString(), parallax);
    auto* parallaxSize = new QLabel(&dialog);
    parallaxSize->setWordWrap(true);
    form->addRow(tr("Required background size:"), parallaxSize);

    auto* stars = new QCheckBox(tr("Show stars"), &dialog);
    stars->setChecked(settings.stars);
    form->addRow(QString(), stars);

    auto percentageSpin = [&dialog]() {
        auto* spin = new QSpinBox(&dialog);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" %"));
        return spin;
    };
    auto* rain = percentageSpin();
    rain->setValue(settings.rainProbability);
    form->addRow(tr("Rain probability:"), rain);
    auto* snow = percentageSpin();
    snow->setValue(settings.snowProbability);
    form->addRow(tr("Snow probability:"), snow);
    auto* bombing = percentageSpin();
    bombing->setValue(settings.bombingProbability);
    form->addRow(tr("Bombing probability:"), bombing);

    auto* civilians = new QSpinBox(&dialog);
    civilians->setRange(0, 1000);
    civilians->setValue(settings.civilians);
    form->addRow(tr("Civilians:"), civilians);
    auto* armed = percentageSpin();
    armed->setValue(settings.armedCiviliansProbability);
    form->addRow(tr("Armed civilians:"), armed);
    layout->addLayout(form);

    auto updateParallaxSize = [=] {
        const auto [requiredWidth, requiredHeight] =
            wingsParallaxSize(width->value(), height->value());
        if (parallax->isChecked()) {
            parallaxSize->setText(
                tr("%1 x %2 px (exact size used by Wings)")
                    .arg(requiredWidth)
                    .arg(requiredHeight));
        } else {
            parallaxSize->setText(tr("Not used"));
        }
        parallaxSize->setEnabled(parallax->isChecked());
    };
    connect(width, &QSpinBox::valueChanged, &dialog,
            [=](int) { updateParallaxSize(); });
    connect(height, &QSpinBox::valueChanged, &dialog,
            [=](int) { updateParallaxSize(); });
    connect(parallax, &QCheckBox::toggled, &dialog,
            [=](bool) { updateParallaxSize(); });
    updateParallaxSize();

    auto* note = new QLabel(
        tr("Wings requires at least 157 x 90 pixels. The 1000 x 1000 "
           "editor limit is provisional because MAKELEV does not document "
           "a maximum size."),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    settings.name = nameEdit->text().trimmed().toStdString();
    if (settings.name.empty()) {
        settings.name = "UNTITLED";
    }
    settings.width = width->value();
    settings.height = height->value();
    settings.backgroundMode = parallax->isChecked()
                                  ? BackgroundMode::Parallax
                                  : BackgroundMode::None;
    settings.stars = stars->isChecked();
    settings.rainProbability = rain->value();
    settings.snowProbability = snow->value();
    settings.bombingProbability = bombing->value();
    settings.civilians = civilians->value();
    settings.armedCiviliansProbability = armed->value();
    return true;
}

bool MainWindow::promptForOverThrustSettings(
    LevelCreationSettings& settings, const bool allowDimensionEditing)
{
    if (!overthrustData_) {
        overthrustData_ = std::make_unique<OverThrustProjectData>();
    }
    OverThrustProjectData edited = *overthrustData_;
    const GameProfile& profile = gameProfile(GameId::OverThrust);
    QDialog dialog(this);
    dialog.setWindowTitle(tr("OverThrust level settings"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* displayName = new QLineEdit(
        QString::fromStdString(edited.displayName), &dialog);
    displayName->setMaxLength(256);
    form->addRow(tr("Level name:"), displayName);
    auto* author = new QLineEdit(QString::fromStdString(edited.author), &dialog);
    author->setMaxLength(256);
    form->addRow(tr("Author:"), author);
    auto* description = new QTextEdit(
        QString::fromStdString(edited.description), &dialog);
    description->setAcceptRichText(false);
    description->setMaximumHeight(90);
    form->addRow(tr("Description:"), description);
    auto* width = new QSpinBox(&dialog);
    width->setRange(profile.minimumWidth, profile.maximumWidth);
    width->setValue(settings.width);
    width->setEnabled(allowDimensionEditing);
    width->setSuffix(tr(" px"));
    form->addRow(tr("World width:"), width);
    auto* height = new QSpinBox(&dialog);
    height->setRange(profile.minimumHeight, profile.maximumHeight);
    height->setValue(settings.height);
    height->setEnabled(allowDimensionEditing);
    height->setSuffix(tr(" px"));
    form->addRow(tr("World height:"), height);
    auto* parallax = new QCheckBox(tr("Parallax imported PNG"), &dialog);
    parallax->setChecked(edited.backgroundParallax);
    form->addRow(QString(), parallax);
    auto* scrollX = new QDoubleSpinBox(&dialog);
    scrollX->setRange(0, 4);
    scrollX->setDecimals(6);
    scrollX->setValue(edited.backgroundScrollX);
    form->addRow(tr("PNG scroll X:"), scrollX);
    auto* scrollY = new QDoubleSpinBox(&dialog);
    scrollY->setRange(0, 4);
    scrollY->setDecimals(6);
    scrollY->setValue(edited.backgroundScrollY);
    form->addRow(tr("PNG scroll Y:"), scrollY);
    auto* backgroundStatus = new QLabel(&dialog);
    backgroundStatus->setWordWrap(true);
    auto updateBackgroundStatus = [&] {
        backgroundStatus->setText(
            edited.background.empty()
                ? tr("None (optional)")
                : tr("%1 × %2, normalized sRGB RGBA8 presentation PNG. Canvas preview is optional and hidden by default.")
                      .arg(edited.background.width)
                      .arg(edited.background.height));
    };
    auto* backgroundRow = new QWidget(&dialog);
    auto* backgroundLayout = new QHBoxLayout(backgroundRow);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->addWidget(backgroundStatus, 1);
    auto* replaceBackground = new QPushButton(tr("Import / replace..."),
                                               backgroundRow);
    backgroundLayout->addWidget(replaceBackground);
    auto* removeBackground = new QPushButton(tr("Remove"), backgroundRow);
    backgroundLayout->addWidget(removeBackground);
    form->addRow(tr("Imported PNG:"), backgroundRow);
    auto* backgroundRequirements = new QLabel(&dialog);
    backgroundRequirements->setWordWrap(false);
    backgroundRequirements->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const auto updateBackgroundRequirements = [&] {
        backgroundRequirements->setText(overthrustBackgroundRequirements(
            static_cast<std::size_t>(width->value()),
            static_cast<std::size_t>(height->value())));
    };
    form->addRow(tr("Image limits:"), backgroundRequirements);
    connect(width, &QSpinBox::valueChanged, &dialog,
            [&](int) { updateBackgroundRequirements(); });
    connect(height, &QSpinBox::valueChanged, &dialog,
            [&](int) { updateBackgroundRequirements(); });
    connect(replaceBackground, &QPushButton::clicked, &dialog, [&] {
        const QString filename = QFileDialog::getOpenFileName(
            &dialog, tr("Import OverThrust background"), QString(),
            tr("Background images (*.png *.PNG *.jpg *.JPG *.jpeg *.JPEG *.webp *.WEBP)"));
        if (filename.isEmpty()) return;
        OverThrustBackground imported;
        QString importError;
        if (!loadOverThrustBackground(filename,
                                      static_cast<std::size_t>(width->value()),
                                      static_cast<std::size_t>(height->value()),
                                      imported, importError)) {
            QMessageBox::critical(&dialog, tr("Background import failed"),
                                  importError + QStringLiteral("\n\n") +
                                      overthrustBackgroundRequirements(
                                          static_cast<std::size_t>(width->value()),
                                          static_cast<std::size_t>(height->value())));
            return;
        }
        edited.background = std::move(imported);
        updateBackgroundStatus();
    });
    connect(removeBackground, &QPushButton::clicked, &dialog, [&] {
        edited.background = {};
        updateBackgroundStatus();
    });
    updateBackgroundRequirements();
    updateBackgroundStatus();
    layout->addLayout(form);

    auto* modes = new QGroupBox(tr("Allowed game modes"), &dialog);
    auto* modesLayout = new QVBoxLayout(modes);
    const std::array<std::pair<const char*, const char*>, 4> modeDefs{{
        {"Sandbox", "core:sandbox"},
        {"Free for all", "core:free_for_all"},
        {"Team deathmatch", "core:team_deathmatch"},
        {"Capture the flag", "core:capture_the_flag"},
    }};
    std::array<QCheckBox*, 4> modeChecks{};
    for (std::size_t index = 0; index < modeDefs.size(); ++index) {
        modeChecks[index] = new QCheckBox(tr(modeDefs[index].first), modes);
        modeChecks[index]->setChecked(
            std::find(edited.allowedGameModes.begin(),
                      edited.allowedGameModes.end(), modeDefs[index].second) !=
            edited.allowedGameModes.end());
        modesLayout->addWidget(modeChecks[index]);
    }
    auto* capturePlayersRow = new QWidget(modes);
    auto* capturePlayersLayout = new QHBoxLayout(capturePlayersRow);
    capturePlayersLayout->setContentsMargins(20, 0, 0, 0);
    auto* capturePlayersLabel = new QLabel(tr("Player count:"),
                                           capturePlayersRow);
    auto* capturePlayers = new QSpinBox(capturePlayersRow);
    capturePlayers->setRange(2, 4);
    capturePlayers->setValue(edited.captureTheFlagPlayers);
    capturePlayersLayout->addWidget(capturePlayersLabel);
    capturePlayersLayout->addWidget(capturePlayers);
    capturePlayersLayout->addStretch();
    modesLayout->addWidget(capturePlayersRow);
    const auto updateCapturePlayers = [=] {
        capturePlayersRow->setVisible(modeChecks[3]->isChecked());
    };
    connect(modeChecks[3], &QCheckBox::toggled, &dialog,
            [=](bool) { updateCapturePlayers(); });
    updateCapturePlayers();
    layout->addWidget(modes);
    auto* note = new QLabel(
        tr("Team modes require 2–4 team-base objects and bindings. Capture the flag also requires one separately placed flag for every team."),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    if (static_cast<std::uint64_t>(width->value()) * height->value() >
        profile.maximumCells) {
        QMessageBox::critical(
            this, tr("Invalid material dimensions"),
            tr("OverThrust permits at most 4,194,304 cells."));
        return false;
    }
    if (!edited.background.empty() &&
        !overthrustBackgroundDimensionsAllowed(
            edited.background.width, edited.background.height,
            static_cast<std::size_t>(width->value()),
            static_cast<std::size_t>(height->value()))) {
        QMessageBox::critical(
            this, tr("Background dimensions no longer fit"),
            tr("Replace the background with one within the pixel-size limits for the new world dimensions."));
        return false;
    }
    edited.displayName = displayName->text().trimmed().toStdString();
    edited.levelId = edited.displayName;
    edited.author = author->text().toStdString();
    edited.description = description->toPlainText().left(4096).toStdString();
    edited.backgroundParallax = parallax->isChecked();
    edited.backgroundScrollX = scrollX->value();
    edited.backgroundScrollY = scrollY->value();
    edited.captureTheFlagPlayers = capturePlayers->value();
    edited.allowedGameModes.clear();
    for (std::size_t index = 0; index < modeDefs.size(); ++index) {
        if (modeChecks[index]->isChecked()) {
            edited.allowedGameModes.emplace_back(modeDefs[index].second);
        }
    }
    if (edited.displayName.empty() || edited.allowedGameModes.empty()) {
        QMessageBox::critical(
            this, tr("Invalid OverThrust settings"),
            edited.displayName.empty()
                ? tr("Level name is required.")
                : tr("At least one game mode is required."));
        return false;
    }
    settings.width = width->value();
    settings.height = height->value();
    settings.name = edited.displayName;
    settings.backgroundMode = edited.background.empty()
                                  ? BackgroundMode::None
                                  : BackgroundMode::ExternalTrueColor;
    *overthrustData_ = std::move(edited);
    return true;
}

void MainWindow::editLevelSettings()
{
    if (creationSettings_.game != GameId::Wings &&
        creationSettings_.game != GameId::OverThrust) {
        return;
    }
    LevelCreationSettings edited = creationSettings_;
    edited.name = level_->name;
    const bool accepted = creationSettings_.game == GameId::Wings
                              ? promptForWingsSettings(edited)
                              : promptForOverThrustSettings(edited);
    if (!accepted) {
        return;
    }
    creationSettings_ = edited;
    level_->name = edited.name;
    configureProfileDocuments();
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromStdString(level_->name));
    }
    nonUndoModified_ = true;
    setModified(true);
    updateOverThrustBackgroundPreview();
    updateWindowTitle();
}

Level* MainWindow::activeLevel() const
{
    if ((creationSettings_.game == GameId::Wings ||
         creationSettings_.game == GameId::OverThrust) &&
        backgroundLevel_ &&
        documentTabs_ != nullptr && documentTabs_->currentIndex() == 1) {
        return backgroundLevel_.get();
    }
    return level_.get();
}

void MainWindow::configureProfileDocuments()
{
    const bool wings = creationSettings_.game == GameId::Wings;
    const bool overthrust = creationSettings_.game == GameId::OverThrust;
    documentTabs_->setVisible(wings || overthrust);
    documentTabs_->setTabText(0, overthrust ? tr("Materials") : tr("Level"));
    documentTabs_->setTabText(
        1, overthrust ? tr("PCX Background") : tr("Background"));
    if (!wings && !overthrust) {
        backgroundLevel_.reset();
        return;
    }

    if (level_->width != static_cast<std::size_t>(creationSettings_.width) ||
        level_->height != static_cast<std::size_t>(creationSettings_.height)) {
        canvas_->forgetLevel(level_.get());
        level_->resizePreservingContent(
            static_cast<std::size_t>(creationSettings_.width),
            static_cast<std::size_t>(creationSettings_.height));
    }

    const bool hasBackground = overthrust ||
        creationSettings_.backgroundMode != BackgroundMode::None;
    documentTabs_->setTabEnabled(1, hasBackground);
    if (!hasBackground) {
        canvas_->forgetLevel(backgroundLevel_.get());
        backgroundLevel_.reset();
        documentTabs_->setCurrentIndex(0);
    } else {
        int width = creationSettings_.width;
        int height = creationSettings_.height;
        if (wings &&
            creationSettings_.backgroundMode == BackgroundMode::Parallax) {
            const auto size = wingsParallaxSize(width, height);
            width = size.first;
            height = size.second;
        }
        if (!backgroundLevel_) {
            backgroundLevel_ = std::make_unique<Level>();
            backgroundLevel_->name = "Background";
            backgroundLevel_->palette = level_->palette;
        }
        if (wings) {
            backgroundLevel_->palette = level_->palette;
        }
        if (backgroundLevel_->width != static_cast<std::size_t>(width) ||
            backgroundLevel_->height != static_cast<std::size_t>(height)) {
            canvas_->forgetLevel(backgroundLevel_.get());
            backgroundLevel_->resizePreservingContent(
                static_cast<std::size_t>(width),
                static_cast<std::size_t>(height));
        }
        initializeBackgroundLayer(*backgroundLevel_);
    }
    switchProfileDocument(documentTabs_->currentIndex());
}

void MainWindow::configurePaletteForGame()
{
    if (paletteGroupCombo_ == nullptr || materialIndexSpinBox_ == nullptr ||
        secondaryIndexSpinBox_ == nullptr) {
        return;
    }
    canvas_->setGame(creationSettings_.game);
    publishAction_->setText(
        creationSettings_.game == GameId::OverThrust
            ? tr("&Publish OverThrust Level...")
            : tr("&Publish LEV..."));
    const bool hasInternalLevelName =
        creationSettings_.game == GameId::VWing;
    levelNameLabel_->setVisible(hasInternalLevelName);
    levelNameEdit_->setVisible(hasInternalLevelName);
    levelNameSeparatorAction_->setVisible(hasInternalLevelName);
    if (hasInternalLevelName) {
        levelNameEdit_->setMaxLength(20);
        levelNameEdit_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[\\x20-\\x7E]{0,20}")),
            levelNameEdit_));
        levelNameEdit_->setToolTip(
            tr("Maximum 20 printable ASCII characters. Ä, Ö, Å and other "
               "non-ASCII characters are not supported by LEV files."));
    }
    materialIndexSpinBox_->setGame(creationSettings_.game);
    secondaryIndexSpinBox_->setGame(creationSettings_.game);

    const QSignalBlocker blocker(paletteGroupCombo_);
    paletteGroupCombo_->clear();
    const auto add = [this](const char* label, const PaletteGroup group) {
        paletteGroupCombo_->addItem(tr(label), static_cast<int>(group));
    };
    const bool overthrustBackground =
        creationSettings_.game == GameId::OverThrust &&
        documentTabs_->currentIndex() == 1;
    if (overthrustBackground) {
        add("All background colors (0-255)", PaletteGroup::AllUsable);
    } else if (creationSettings_.game == GameId::OverThrust) {
        add("All material and fly-through indices (0-255)",
            PaletteGroup::AllUsable);
        add("Empty (0)", PaletteGroup::Background);
        add("Structural", PaletteGroup::Structural);
        add("Organic", PaletteGroup::Organic);
        add("Metal", PaletteGroup::Metal);
        add("Granular", PaletteGroup::Granular);
        add("Liquid", PaletteGroup::Liquid);
        add("Gas", PaletteGroup::Gas);
        add("Gameplay", PaletteGroup::Gameplay);
        add("Fly-through colors (97-255)", PaletteGroup::FlyThrough);
    } else if (creationSettings_.game == GameId::Wings) {
        add("All documented usable", PaletteGroup::AllUsable);
        add("Background (0)", PaletteGroup::Background);
        add("Water, bubbles and snow (16, 48-53)", PaletteGroup::Water);
        add("Bases (32-47)", PaletteGroup::Bases);
        add("Fire and explosives (54-56)", PaletteGroup::Special);
        add("Fly-through background (64-79)", PaletteGroup::FlyThrough);
        add("Indestructible (80-95)", PaletteGroup::Indestructible);
        add("Soft terrain (96-111)", PaletteGroup::Soft);
        add("Burning terrain (112-127)", PaletteGroup::BurningWings);
        add("Normal terrain (128-255)", PaletteGroup::NormalTerrain);
    } else if (creationSettings_.game == GameId::Auts) {
        add("All palette indices (0-255)", PaletteGroup::AllUsable);
        add("Space (0)", PaletteGroup::Background);
        add("Indestructible (7)", PaletteGroup::Indestructible);
        add("Water (39)", PaletteGroup::Water);
        add("Docking plate (92-95)", PaletteGroup::Docking);
        add("Other colors", PaletteGroup::Other);
    } else {
        add("All documented usable", PaletteGroup::AllUsable);
        add("Background (0)", PaletteGroup::Background);
        add("Water (16-19)", PaletteGroup::Water);
        add("Fly through (20-30)", PaletteGroup::FlyThrough);
        add("Font (32-37)", PaletteGroup::Font);
        add("Special materials and game colors (39-56)",
            PaletteGroup::Special);
        add("Normal terrain (57-149)", PaletteGroup::NormalTerrain);
        add("Burnable (150-199)", PaletteGroup::Burnable);
        add("Underwater (201-219)", PaletteGroup::Underwater);
        add("Indestructible (221-243, 248-255)",
            PaletteGroup::Indestructible);
        add("Turret colors (244-247; not placement)",
            PaletteGroup::Turrets);
    }
    backgroundPreviewCheck_->setVisible(
        creationSettings_.game == GameId::OverThrust &&
        documentTabs_->currentIndex() == 0);
    paletteWidget_->setIndices(
        paletteIndices(PaletteGroup::AllUsable, creationSettings_.game));
    const int defaultIndex = creationSettings_.game == GameId::Wings
                                 ? 128
                                 : creationSettings_.game == GameId::Auts
                                       ? 7
                                       : creationSettings_.game ==
                                                 GameId::OverThrust
                                             ? 1
                                             : 56;
    const int secondaryIndex = creationSettings_.game == GameId::Auts
                                   ? 39
                                   : defaultIndex + 1;
    materialIndexSpinBox_->setValue(defaultIndex);
    secondaryIndexSpinBox_->setValue(secondaryIndex);
    updateMaterialDetails(defaultIndex);
    updateOverThrustBackgroundPreview();
    if (objectDock_ != nullptr) {
        refreshObjectList();
    }
}

void MainWindow::updateOverThrustBackgroundPreview()
{
    if (creationSettings_.game == GameId::OverThrust &&
        documentTabs_->currentIndex() == 0 && overthrustData_ &&
        !overthrustData_->background.empty() && backgroundPreviewCheck_ &&
        backgroundPreviewCheck_->isChecked()) {
        canvas_->setTrueColorBackground(
            overthrustData_->background.rgba.data(),
            static_cast<int>(overthrustData_->background.width),
            static_cast<int>(overthrustData_->background.height));
    } else {
        canvas_->setTrueColorBackground(nullptr, 0, 0);
    }
}

void MainWindow::switchProfileDocument(const int index)
{
    if ((creationSettings_.game != GameId::Wings &&
         creationSettings_.game != GameId::OverThrust) ||
        level_ == nullptr) {
        return;
    }
    canvas_->commitSelection();
    Level* document = index == 1 && backgroundLevel_ ? backgroundLevel_.get()
                                                     : level_.get();
    canvas_->setLevel(document);
    paletteWidget_->setLevel(document);
    configurePaletteForGame();
    refreshLayerList();
    setModified(nonUndoModified_ || canvas_->hasDirtyUndoStack());
    const bool overthrust = creationSettings_.game == GameId::OverThrust;
    statusBar()->showMessage(
        index == 1
            ? (overthrust ? tr("Editing OverThrust PCX background: %1 x %2")
                              : tr("Editing Wings background: %1 x %2"))
                  .arg(document->width)
                  .arg(document->height)
            : (overthrust ? tr("Editing OverThrust materials: %1 x %2")
                              : tr("Editing Wings level: %1 x %2"))
                  .arg(document->width)
                  .arg(document->height),
        3000);
}

void MainWindow::updateMaterialDetails(const int index)
{
    const bool overthrustBackground =
        creationSettings_.game == GameId::OverThrust &&
        documentTabs_->currentIndex() == 1;
    const RGB& color =
        activeLevel()->palette[static_cast<std::size_t>(index)];
    const QString hex =
        QStringLiteral("#%1%2%3")
            .arg(static_cast<int>(color.r), 2, 16, QLatin1Char('0'))
            .arg(static_cast<int>(color.g), 2, 16, QLatin1Char('0'))
            .arg(static_cast<int>(color.b), 2, 16, QLatin1Char('0'))
            .toUpper();
    QString details;
    if (overthrustBackground) {
        details = tr("Background color index %1 / file index %2\n"
                     "RGB: %3, %4, %5   %6")
                      .arg(colorChartNumber(index))
                      .arg(index)
                      .arg(static_cast<int>(color.r))
                      .arg(static_cast<int>(color.g))
                      .arg(static_cast<int>(color.b))
                      .arg(hex);
    } else {
        details = tr("Material index %1 / file index %2\n"
                     "RGB: %3, %4, %5   %6\n%7")
                      .arg(colorChartNumber(index))
                      .arg(index)
                      .arg(static_cast<int>(color.r))
                      .arg(static_cast<int>(color.g))
                      .arg(static_cast<int>(color.b))
                      .arg(hex)
                      .arg(materialDescription(creationSettings_.game,
                                               index));
    }
    materialDetailsLabel_->setText(details);
    const bool editable = overthrustBackground ||
        isPaletteColorEditable(creationSettings_.game, index);
    editColorButton_->setEnabled(editable);
    if (editable) {
        editColorButton_->setToolTip(
            overthrustBackground
                ? tr("Change this color only in the PCX background palette")
                : tr("Change this palette color globally"));
    } else if (creationSettings_.game == GameId::Auts) {
        editColorButton_->setToolTip(tr("AUTS uses a fixed game palette"));
    } else {
        editColorButton_->setToolTip(
            tr("This palette color is fixed or reserved by the game"));
    }
}

bool MainWindow::saveProject()
{
    return projectPath_.empty() ? saveProjectAs() : writeProject(projectPath_);
}

bool MainWindow::saveProjectAs()
{
    QString suggested = toQString(projectPath_);
    if (suggested.isEmpty()) {
        QSettings settings;
        const QString directory =
            effectiveProjectDirectorySetting(settings);
        if (QDir(directory).exists() || QDir().mkpath(directory)) {
            QString projectName = QString::fromLatin1(
                creationSettings_.name.data(),
                static_cast<qsizetype>(creationSettings_.name.size()));
            projectName.replace(
                QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")),
                QStringLiteral("_"));
            if (projectName.isEmpty()) {
                projectName = tr("Untitled");
            }
            suggested = QDir(directory).filePath(
                projectName + QStringLiteral(".pxlp"));
        }
    }
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save editable PCX Level Tool project"),
        suggested, tr("PCX Level Tool projects (*.pxlp)"));
    if (filename.isEmpty()) {
        return false;
    }
    if (!filename.endsWith(QStringLiteral(".pxlp"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".pxlp");
    }
    return writeProject(toPath(filename));
}

bool MainWindow::maybeSave()
{
    if (!modified_) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes to the editable project?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        return saveProject();
    }
    return choice == QMessageBox::Discard;
}

bool MainWindow::writeProject(const std::filesystem::path& path)
{
    canvas_->commitSelection();
    std::string error;
    bool saved = false;
    if (creationSettings_.game == GameId::VWing ||
        creationSettings_.game == GameId::Auts) {
        uppercaseLevelName();
    }
    flattenLayers(*level_);
    if (backgroundLevel_) {
        flattenLayers(*backgroundLevel_);
    }
    if (overthrustData_) {
        rebuildTeamModeSettings(*overthrustData_);
    }
    saved = savePxlProject(path, *level_, backgroundLevel_.get(),
                           creationSettings_, overthrustData_.get(), error);
    if (!saved) {
        QMessageBox::critical(this, tr("Save failed"),
                              QString::fromStdString(error));
        return false;
    }
    projectPath_ = path;
    canvas_->markAllUndoStacksClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Saved %1").arg(toQString(path)), 3000);
    return true;
}

bool MainWindow::publishLevel()
{
    std::filesystem::path suggested = publishPath_;
    if (creationSettings_.game == GameId::OverThrust) {
        const std::string levelName =
            overthrustData_ && !overthrustData_->displayName.empty()
                ? overthrustData_->displayName
                : "untitled";
        std::string safeName = levelName;
        std::replace_if(safeName.begin(), safeName.end(),
                        [](const char value) {
                            return !(std::isalnum(
                                         static_cast<unsigned char>(value)) ||
                                     value == '-' || value == '_' ||
                                     value == '.');
                        },
                        '_');
        if (suggested.empty()) {
            suggested = std::filesystem::path(safeName + ".OLV");
        }
        QSettings settings;
        const QString configuredDirectory =
            exportDirectorySetting(settings, GameId::OverThrust);
        if (!configuredDirectory.isEmpty() &&
            QDir(configuredDirectory).exists()) {
            suggested = toPath(configuredDirectory) / suggested.filename();
        }
        QString filename = QFileDialog::getSaveFileName(
            this, tr("Publish OverThrust level package"),
            toQString(suggested),
            tr("OverThrust level packages (*.OLV *.olv)"));
        if (filename.isEmpty()) return false;
        if (!filename.endsWith(QStringLiteral(".olv"),
                               Qt::CaseInsensitive)) {
            filename += QStringLiteral(".OLV");
        }
        return writePublishedLevel(toPath(filename));
    }
    if ((creationSettings_.game == GameId::Wings ||
         creationSettings_.game == GameId::Auts) &&
        suggested.empty()) {
        suggested = std::filesystem::path(creationSettings_.name + ".LEV");
    }
    if (suggested.empty() && !projectPath_.empty()) {
        suggested = projectPath_;
        suggested.replace_extension(".LEV");
    }
    if (suggested.empty() && !creationSettings_.name.empty()) {
        suggested = std::filesystem::path(creationSettings_.name + ".LEV");
    }
    QSettings settings;
    const QString configuredDirectory =
        exportDirectorySetting(settings, creationSettings_.game);
    if (!configuredDirectory.isEmpty() &&
        QDir(configuredDirectory).exists()) {
        const std::filesystem::path directory = toPath(configuredDirectory);
        suggested = suggested.empty()
                        ? directory
                        : directory / suggested.filename();
    }
    QString gameName;
    if (creationSettings_.game == GameId::Wings) {
        gameName = QStringLiteral("Wings");
    } else if (creationSettings_.game == GameId::Auts) {
        gameName = QStringLiteral("AUTS");
    } else {
        gameName = QStringLiteral("V-Wing");
    }
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Publish game-compatible %1 level").arg(gameName),
        toQString(suggested), tr("%1 levels (*.LEV)").arg(gameName));
    if (filename.isEmpty()) {
        return false;
    }
    if (!filename.endsWith(QStringLiteral(".lev"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".LEV");
    }
    return writePublishedLevel(toPath(filename));
}

bool MainWindow::writePublishedLevel(const std::filesystem::path& path)
{
    canvas_->commitSelection();
    if (creationSettings_.game == GameId::OverThrust) {
        if (!overthrustData_) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  tr("OverThrust project data is missing."));
            return false;
        }
        flattenLayers(*level_);
        if (!backgroundLevel_) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  tr("OverThrust PCX background is missing."));
            return false;
        }
        flattenLayers(*backgroundLevel_);
        rebuildTeamModeSettings(*overthrustData_);
        std::string error;
        if (!publishOverThrustPackage(path, *level_, *backgroundLevel_,
                                      *overthrustData_, error)) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  QString::fromStdString(error));
            return false;
        }
        publishPath_ = path;
        statusBar()->showMessage(tr("Published %1").arg(toQString(path)),
                                 3000);
        return true;
    }
    if (creationSettings_.game == GameId::Wings) {
        flattenLayers(*level_);
        if (backgroundLevel_) {
            flattenLayers(*backgroundLevel_);
        }
        std::string error;
        if (!saveWingsLev(path, *level_, backgroundLevel_.get(),
                          creationSettings_, error)) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  QString::fromStdString(error));
            return false;
        }
        publishPath_ = path;
        statusBar()->showMessage(tr("Published %1").arg(toQString(path)),
                                 3000);
        return true;
    }
    if (creationSettings_.game == GameId::Auts) {
        uppercaseLevelName();
        flattenLayers(*level_);
        std::string error;
        if (!saveAutsLev(path, *level_, error)) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  QString::fromStdString(error));
            return false;
        }
        publishPath_ = path;
        statusBar()->showMessage(tr("Published %1").arg(toQString(path)),
                                 3000);
        return true;
    }
    uppercaseLevelName();
    canvas_->refreshImage();
    std::string error;
    if (!saveLev(path, *level_, error)) {
        QMessageBox::critical(this, tr("Publish failed"),
                              QString::fromStdString(error));
        return false;
    }
    publishPath_ = path;
    statusBar()->showMessage(tr("Published %1").arg(toQString(path)), 3000);
    return true;
}

void MainWindow::uppercaseLevelName()
{
    if (creationSettings_.game == GameId::Auts) {
        level_->name.erase(
            std::remove_if(level_->name.begin(), level_->name.end(),
                           [](const char character) {
                               const unsigned char value =
                                   static_cast<unsigned char>(character);
                               return !(std::isalnum(value) || character == '_' ||
                                        character == '-');
                           }),
            level_->name.end());
        if (level_->name.size() > 8) {
            level_->name.resize(8);
        }
        if (level_->name.empty()) {
            level_->name = "UNTITLED";
        }
    } else if (creationSettings_.game == GameId::VWing &&
               level_->name.size() > 20) {
        level_->name.resize(20);
    }
    std::transform(level_->name.begin(), level_->name.end(),
                   level_->name.begin(), [](const char character) {
                       return character >= 'a' && character <= 'z'
                                  ? static_cast<char>(character - 'a' + 'A')
                                  : character;
                   });
    creationSettings_.name = level_->name;
    const QSignalBlocker blocker(levelNameEdit_);
    levelNameEdit_->setText(
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size())));
    updateWindowTitle();
}

void MainWindow::editSelectedPaletteColor()
{
    const int index = paletteIndexFromColorChart(
        materialIndexSpinBox_->value());
    const bool overthrustBackground =
        creationSettings_.game == GameId::OverThrust &&
        documentTabs_->currentIndex() == 1;
    if (!overthrustBackground &&
        !isPaletteColorEditable(creationSettings_.game, index)) {
        return;
    }
    const RGB& current =
        activeLevel()->palette[static_cast<std::size_t>(index)];
    const QColor selected =
        QColorDialog::getColor(QColor(current.r, current.g, current.b), this,
                               (overthrustBackground
                                    ? tr("Background color index %1 (file index %2)")
                                    : tr("Material index %1 (file index %2)"))
                                   .arg(colorChartNumber(index))
                                   .arg(index));
    if (!selected.isValid()) {
        return;
    }
    canvas_->setPaletteColor(static_cast<std::uint8_t>(index),
                             RGB{static_cast<std::uint8_t>(selected.red()),
                                 static_cast<std::uint8_t>(selected.green()),
                                 static_cast<std::uint8_t>(selected.blue())});
}

void MainWindow::loadPalette()
{
    if (creationSettings_.game == GameId::Auts) {
        QMessageBox::information(
            this, tr("Fixed AUTS palette"),
            tr("AUTS uses the fixed palette supplied with its original "
               "converter. Custom palettes cannot be loaded for AUTS levels."));
        return;
    }
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load palette"), QString(), tr("JASC palettes (*.pal)"));
    if (filename.isEmpty()) {
        return;
    }

    std::array<RGB, 256> palette{};
    std::string error;
    if (!loadJascPalette(toPath(filename), palette, error)) {
        QMessageBox::critical(this, tr("Palette load failed"),
                              QString::fromStdString(error));
        return;
    }
    if (creationSettings_.game == GameId::Wings) {
        const auto fixed = defaultWingsPalette();
        std::copy_n(fixed.begin(), 48, palette.begin());
        const auto& current = activeLevel()->palette;
        for (int index = 48; index < 256; ++index) {
            if (isReservedPaletteIndex(GameId::Wings, index)) {
                palette[static_cast<std::size_t>(index)] =
                    current[static_cast<std::size_t>(index)];
            }
        }
    }
    canvas_->setPalette(palette);
    statusBar()->showMessage(tr("Loaded palette %1").arg(filename), 3000);
}

void MainWindow::savePalette()
{
    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save palette"), QString(), tr("JASC palettes (*.pal)"));
    if (filename.isEmpty()) {
        return;
    }

    std::string error;
    if (!saveJascPalette(toPath(filename), activeLevel()->palette, error)) {
        QMessageBox::critical(this, tr("Palette save failed"),
                              QString::fromStdString(error));
        return;
    }
    statusBar()->showMessage(tr("Saved palette %1").arg(filename), 3000);
}

void MainWindow::setModified(const bool modified)
{
    modified_ = modified;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString filename = projectPath_.empty()
                                 ? (publishPath_.empty()
                                        ? tr("Untitled")
                                        : toQString(publishPath_.filename()))
                                 : toQString(projectPath_.filename());
    const QString levelName =
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size()));
    const QString document = levelName.isEmpty()
                                 ? filename
                                 : tr("%1 [%2]").arg(filename, levelName);
    const GameProfile& profile = gameProfile(creationSettings_.game);
    const QString game = QString::fromUtf8(profile.displayName.data(),
                                           profile.displayName.size());
    setWindowTitle(tr("%1%2 — %3 — PCX Level Tool")
                       .arg(modified_ ? QStringLiteral("*") : QString(),
                            document, game));
}
