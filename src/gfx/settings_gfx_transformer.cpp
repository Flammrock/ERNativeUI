#include "gfx/settings_gfx_transformer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace erui::gfx {
namespace {

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineEditText = 37;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kTagPlaceObject3 = 70;
constexpr std::uint16_t kTagDefineScalingGrid = 78;
constexpr std::uint16_t kTagDefineShape4 = 83;
constexpr std::uint16_t kTagDefineExternalImage2 = 1009;

constexpr std::uint16_t kMinimumGameRows = 6;
constexpr std::uint16_t kMinimumCameraRows = 7;
constexpr std::uint16_t kMaximumRows = 13;
constexpr std::uint16_t kMinimumAuthoredTabCells = 9;
constexpr std::uint16_t kMinimumRequestedTabSlots = 8;
constexpr std::uint16_t kMaximumRequestedTabSlots = 10;
constexpr std::size_t kMaximumAuthoredTabCells = 256;
// A settings movie normally exposes only a handful of widget libraries. Keep
// discovery bounded so a hostile replacement cannot amplify one shared Button
// into tens of thousands of generated ColorPicker definitions on the game's
// UI thread.
constexpr std::size_t kMaximumWidgetHosts = 256;
constexpr std::string_view kGamePanel = "ControllSetting";
constexpr std::string_view kCameraPanel = "CameraSetting";
constexpr std::string_view kDisplayPanel = "EnvironmentSetting";
constexpr std::string_view kSoundPanel = "AudioSetting";
constexpr std::string_view kNetworkPanel = "NetworkSetting";
constexpr std::string_view kKeyboardMousePanel = "PCMouseKey";
constexpr std::string_view kGraphicsPanel = "PCGraphic";
constexpr std::string_view kTabList = "TabList";
constexpr std::string_view kBackTabList = "BackTabList";
constexpr std::string_view kLeftArrow = "LeftArrow";
constexpr std::string_view kRightArrow = "RightArrow";
constexpr std::string_view kCharacterFrame = "CharacterFrame";
constexpr std::string_view kValueText = "Text_0";
constexpr std::string_view kEmptyText = "TextOnEmpty";
constexpr std::string_view kCaption = "Caption";
constexpr std::string_view kCursor = "Cursor";
constexpr std::string_view kButton = "Button";
constexpr std::string_view kTextInput = "TextInput";
constexpr std::string_view kComboBox = "ComboBox";
constexpr std::string_view kSlider = "Slider";
constexpr std::string_view kColorPicker = "ColorPicker";
constexpr std::string_view kButtonFrame = "ButtonFrame";
constexpr std::string_view kLegacyPreview = "ColorPreview";
constexpr std::string_view kColor = "Color";
constexpr std::string_view kCharacterFrameExport = "MENU_FL_Cursor_EntWaku";
constexpr std::string_view kCharacterFrameFile = "MENU_FL_Cursor_EntWaku.tga";
constexpr std::string_view kColorFrameExport = "MENU_FL_ColorWaku";
constexpr std::string_view kColorFrameFile = "MENU_FL_ColorWaku.tga";

struct SpriteView {
    GfxSpriteInfo info{};
    std::vector<const GfxTagInfo*> tags{};
    std::vector<const GfxPlacementInfo*> placements{};
};

class Model {
public:
    explicit Model(const GfxDocument& document) : document_(document) {
        sprites_.reserve(document.sprites().size());
        for (const auto& info : document.sprites()) {
            if (info.parent_sprite) continue;
            const std::size_t index = sprites_.size();
            sprites_.push_back({info, {}, {}});
            definition_indices_.emplace(info.definition.token, index);
            character_indices_.emplace(info.character_id, index);
        }
        for (const auto& tag : document.tags()) {
            if (!tag.parent_sprite) continue;
            if (auto* owner = by_definition(tag.parent_sprite)) owner->tags.push_back(&tag);
        }
        for (const auto& placement : document.placements()) {
            if (auto* owner = by_definition(placement.owner_sprite)) {
                owner->placements.push_back(&placement);
            }
        }
    }

    [[nodiscard]] const GfxDocument& document() const noexcept { return document_; }
    [[nodiscard]] const std::vector<SpriteView>& sprites() const noexcept { return sprites_; }
    [[nodiscard]] const SpriteView* sprite(std::uint16_t id) const noexcept {
        const auto found = character_indices_.find(id);
        return found == character_indices_.end()
            ? nullptr
            : &sprites_[found->second];
    }
    [[nodiscard]] const GfxPlacementInfo* unique(
        const SpriteView& sprite, std::string_view name) const noexcept {
        const GfxPlacementInfo* result{};
        for (const auto* placement : sprite.placements) {
            if (!placement->has_name || placement->name != name) continue;
            if (result) return nullptr;
            result = placement;
        }
        return result;
    }
    [[nodiscard]] const GfxPlacementInfo* at_depth(
        const SpriteView& sprite, std::uint16_t depth) const noexcept {
        const GfxPlacementInfo* result{};
        for (const auto* placement : sprite.placements) {
            if (!placement->has_character || placement->depth != depth) continue;
            if (result) return nullptr;
            result = placement;
        }
        return result;
    }
    [[nodiscard]] bool any_placement_at_depth(
        const SpriteView& sprite,
        std::uint16_t depth) const noexcept {
        return std::any_of(
            sprite.placements.begin(),
            sprite.placements.end(),
            [depth](const GfxPlacementInfo* placement) {
                return placement->depth == depth;
            });
    }
    [[nodiscard]] const GfxTagInfo* show_frame(const SpriteView& sprite) const noexcept {
        const GfxTagInfo* result{};
        for (const auto* tag : sprite.tags) {
            if (tag->code != kTagShowFrame) continue;
            if (result) return nullptr;
            result = tag;
        }
        return result;
    }
    [[nodiscard]] const GfxTagInfo* first_show_frame(
        const SpriteView& sprite) const noexcept {
        const auto found = std::find_if(
            sprite.tags.begin(), sprite.tags.end(),
            [](const GfxTagInfo* tag) {
                return tag->code == kTagShowFrame;
            });
        return found == sprite.tags.end() ? nullptr : *found;
    }
    [[nodiscard]] const GfxTagInfo* top_level_end() const noexcept {
        const GfxTagInfo* result{};
        for (const auto& tag : document_.tags()) {
            if (tag.parent_sprite || tag.code != kTagEnd) continue;
            if (result) return nullptr;
            result = &tag;
        }
        return result;
    }

private:
    [[nodiscard]] SpriteView* by_definition(GfxTagHandle handle) noexcept {
        const auto found = definition_indices_.find(handle.token);
        return found == definition_indices_.end()
            ? nullptr
            : &sprites_[found->second];
    }
    const GfxDocument& document_;
    std::vector<SpriteView> sprites_{};
    std::unordered_map<std::uint64_t, std::size_t> definition_indices_{};
    std::unordered_map<std::uint16_t, std::size_t> character_indices_{};
};

struct PanelContext {
    const SpriteView* window_list{};
    const SpriteView* panel{};
    std::uint16_t item_character{};
    std::uint16_t row_count{};
    std::vector<const GfxPlacementInfo*> items{};
};

struct TabLayerContext {
    const SpriteView* sprite{};
    std::vector<const GfxPlacementInfo*> items{};
};

struct TabStripContext {
    TabLayerContext tab_list{};
    TabLayerContext back_tab_list{};
    std::uint16_t cell_count{};
};

enum class PanelCharacterPolicy : std::uint8_t {
    uniform,
    trailing_spare,
};

using RowRequirementMember =
    std::optional<std::uint16_t> SettingsGfxRequirements::*;
using InspectionMember = std::uint16_t SettingsGfxInspection::*;

struct PanelContract {
    std::string_view name{};
    std::uint16_t minimum_rows{};
    SettingsGfxFeature feature{SettingsGfxFeature::none};
    PanelCharacterPolicy character_policy{PanelCharacterPolicy::uniform};
    RowRequirementMember requirement{};
    InspectionMember sprite{};
    InspectionMember item_character{};
    InspectionMember rows{};
};

constexpr std::array<PanelContract, 7> kPanelContracts{{
    {kGamePanel, kMinimumGameRows, SettingsGfxFeature::game_options_rows,
        PanelCharacterPolicy::uniform,
        &SettingsGfxRequirements::minimum_game_options_rows,
        &SettingsGfxInspection::game_options_sprite,
        &SettingsGfxInspection::game_options_item_character,
        &SettingsGfxInspection::game_options_rows},
    {kCameraPanel, kMinimumCameraRows,
        SettingsGfxFeature::camera_options_rows,
        PanelCharacterPolicy::uniform,
        &SettingsGfxRequirements::minimum_camera_options_rows,
        &SettingsGfxInspection::camera_options_sprite,
        &SettingsGfxInspection::camera_options_item_character,
        &SettingsGfxInspection::camera_options_rows},
    {kDisplayPanel, 12, SettingsGfxFeature::display_rows,
        PanelCharacterPolicy::uniform,
        &SettingsGfxRequirements::minimum_display_rows,
        &SettingsGfxInspection::display_sprite,
        &SettingsGfxInspection::display_item_character,
        &SettingsGfxInspection::display_rows},
    {kSoundPanel, 12, SettingsGfxFeature::sound_rows,
        PanelCharacterPolicy::uniform,
        &SettingsGfxRequirements::minimum_sound_rows,
        &SettingsGfxInspection::sound_sprite,
        &SettingsGfxInspection::sound_item_character,
        &SettingsGfxInspection::sound_rows},
    {kNetworkPanel, 7, SettingsGfxFeature::network_rows,
        PanelCharacterPolicy::uniform,
        &SettingsGfxRequirements::minimum_network_rows,
        &SettingsGfxInspection::network_sprite,
        &SettingsGfxInspection::network_item_character,
        &SettingsGfxInspection::network_rows},
    {kKeyboardMousePanel, 7, SettingsGfxFeature::keyboard_mouse_rows,
        PanelCharacterPolicy::trailing_spare,
        &SettingsGfxRequirements::minimum_keyboard_mouse_rows,
        &SettingsGfxInspection::keyboard_mouse_sprite,
        &SettingsGfxInspection::keyboard_mouse_item_character,
        &SettingsGfxInspection::keyboard_mouse_rows},
    {kGraphicsPanel, 7, SettingsGfxFeature::graphics_rows,
        PanelCharacterPolicy::trailing_spare,
        &SettingsGfxRequirements::minimum_graphics_rows,
        &SettingsGfxInspection::graphics_sprite,
        &SettingsGfxInspection::graphics_item_character,
        &SettingsGfxInspection::graphics_rows},
}};

struct TextInputContext {
    const SpriteView* sprite{};
    const GfxPlacementInfo* value{};
    const GfxPlacementInfo* empty{};
    const GfxPlacementInfo* caption{};
    const GfxPlacementInfo* cursor{};
    SettingsGfxHost host{SettingsGfxHost::unknown};
};

struct WidgetHost {
    std::uint16_t container{};
    std::uint16_t button{};
};

struct WidgetContext {
    SettingsGfxHost host{SettingsGfxHost::unknown};
    std::vector<WidgetHost> hosts{};
    std::vector<std::uint16_t> buttons{};
};

[[nodiscard]] std::optional<GfxCharacterDefinitionInfo>
top_level_character_definition(
    const GfxDocument& document,
    std::uint16_t character_id) noexcept {
    const auto definition = document.character_definition(character_id);
    if (!definition) return std::nullopt;
    const auto tag = document.tag(definition->tag);
    if (!tag || tag->parent_sprite) return std::nullopt;
    return definition;
}

[[nodiscard]] std::optional<GfxTagInfo> top_level_tag(
    const GfxDocument& document,
    GfxTagHandle handle) noexcept {
    const auto tag = document.tag(handle);
    if (!tag || tag->parent_sprite) return std::nullopt;
    return tag;
}

[[nodiscard]] std::optional<GfxTagHandle> earliest_widget_user(
    const GfxDocument& document,
    const Model& model,
    const WidgetContext& widgets) noexcept {
    std::optional<GfxTagInfo> earliest{};
    for (const auto& host : widgets.hosts) {
        for (const std::uint16_t id : {host.button, host.container}) {
            const auto* sprite = model.sprite(id);
            if (!sprite) return std::nullopt;
            const auto tag = top_level_tag(document, sprite->info.definition);
            if (!tag) return std::nullopt;
            if (!earliest || tag->ordinal < earliest->ordinal) earliest = tag;
        }
    }
    return earliest ? std::optional<GfxTagHandle>{earliest->handle}
                    : std::nullopt;
}

[[nodiscard]] std::optional<GfxTagHandle> earliest_color_picker_user(
    const GfxDocument& document,
    const Model& model,
    const TextInputContext& text,
    const WidgetContext& widgets) noexcept {
    if (!text.sprite) return std::nullopt;
    const auto text_user = top_level_tag(
        document, text.sprite->info.definition);
    const auto widget_user = earliest_widget_user(
        document, model, widgets);
    const auto widget_tag = widget_user
        ? top_level_tag(document, *widget_user)
        : std::nullopt;
    if (!text_user || !widget_tag) return std::nullopt;
    return text_user->ordinal < widget_tag->ordinal
        ? std::optional<GfxTagHandle>{text_user->handle}
        : widget_user;
}

[[nodiscard]] SettingsGfxError semantic_error(
    SettingsGfxErrorCode code, SettingsGfxFeature feature,
    std::uint32_t detail = 0) noexcept {
    return {.code = code, .feature = feature, .detail = detail};
}

[[nodiscard]] SettingsGfxError document_error(
    SettingsGfxFeature feature, GfxDocumentError error) noexcept {
    return {.code = SettingsGfxErrorCode::mutation_failed,
        .feature = feature, .document_error = error};
}

[[nodiscard]] std::optional<std::uint16_t> item_index(std::string_view name) noexcept {
    constexpr std::string_view prefix = "Item_";
    constexpr std::string_view suffix = "_0";
    if (!name.starts_with(prefix) || !name.ends_with(suffix) ||
        name.size() <= prefix.size() + suffix.size()) return std::nullopt;
    const auto digits = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    std::uint16_t result{};
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) return std::nullopt;
    return result;
}

[[nodiscard]] std::optional<std::uint16_t> tab_item_index(
    std::string_view name) noexcept {
    constexpr std::string_view prefix = "Item_0_";
    if (!name.starts_with(prefix) || name.size() == prefix.size()) {
        return std::nullopt;
    }
    const auto digits = name.substr(prefix.size());
    std::uint16_t result{};
    const auto parsed = std::from_chars(
        digits.data(), digits.data() + digits.size(), result);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] SettingsGfxError locate_tab_layer(
    const Model& model,
    std::string_view layer_name,
    TabLayerContext& output) noexcept {
    output = {};
    const GfxPlacementInfo* root{};
    std::size_t references{};
    for (const auto& placement : model.document().placements()) {
        if (placement.owner_sprite || !placement.has_name ||
            placement.name != layer_name) {
            continue;
        }
        ++references;
        root = &placement;
    }
    if (references != 1U || root == nullptr) {
        return semantic_error(
            references == 0U
                ? SettingsGfxErrorCode::structure_not_found
                : SettingsGfxErrorCode::ambiguous_structure,
            SettingsGfxFeature::tab_slots,
            static_cast<std::uint32_t>(references));
    }
    if (root->tag_code != kTagPlaceObject2 || root->move ||
        !root->has_character || !root->has_matrix) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            1);
    }

    output.sprite = model.sprite(root->character_id);
    if (output.sprite == nullptr) {
        return semantic_error(
            SettingsGfxErrorCode::structure_not_found,
            SettingsGfxFeature::tab_slots,
            2);
    }
    if (output.sprite->info.frame_count != 1U ||
        model.show_frame(*output.sprite) == nullptr) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            3);
    }

    std::unordered_set<std::uint16_t> depths{};
    depths.reserve(output.sprite->placements.size());
    std::unordered_map<std::uint16_t, const GfxPlacementInfo*> indexed{};
    indexed.reserve(output.sprite->placements.size());
    for (const auto* placement : output.sprite->placements) {
        if (!depths.emplace(placement->depth).second) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                4);
        }
        if (!placement->has_name) continue;
        const auto index = tab_item_index(placement->name);
        if (!index) continue;
        // Vanilla uses 0x26 in TabList and 0x2e in BackTabList. The latter
        // carries an authored color transform. Both layouts are safe to clone
        // because the remaining optional fields are preserved byte-for-byte.
        if (placement->tag_code != kTagPlaceObject2 || placement->move ||
            (placement->flags != 0x26U && placement->flags != 0x2EU) ||
            !placement->has_character || !placement->has_matrix ||
            !indexed.emplace(*index, placement).second) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                5);
        }
    }
    if (indexed.size() < kMinimumAuthoredTabCells ||
        indexed.size() > kMaximumAuthoredTabCells) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            static_cast<std::uint32_t>(indexed.size()));
    }
    const auto maximum = std::max_element(
        indexed.begin(), indexed.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    if (maximum == indexed.end() ||
        maximum->first == std::numeric_limits<std::uint16_t>::max()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            6);
    }
    const auto count = static_cast<std::uint16_t>(maximum->first + 1U);
    if (count != indexed.size()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            7);
    }
    output.items.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto found = indexed.find(index);
        if (found == indexed.end()) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                8);
        }
        if (index != 0U &&
            found->second->translation.x <=
                output.items.back()->translation.x) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                9);
        }
        if (index != 0U &&
            found->second->translation.y !=
                output.items.front()->translation.y) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                10);
        }
        output.items.push_back(found->second);
    }
    return {};
}

[[nodiscard]] SettingsGfxError locate_tab_strip(
    const Model& model,
    TabStripContext& output) noexcept {
    output = {};
    if (auto error = locate_tab_layer(
            model, kTabList, output.tab_list);
        !error.success()) {
        return error;
    }
    if (auto error = locate_tab_layer(
            model, kBackTabList, output.back_tab_list);
        !error.success()) {
        return error;
    }
    if (output.tab_list.sprite == output.back_tab_list.sprite ||
        output.tab_list.items.size() != output.back_tab_list.items.size()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            11);
    }
    for (std::size_t index = 0;
         index < output.tab_list.items.size();
         ++index) {
        const auto* front = output.tab_list.items[index];
        const auto* back = output.back_tab_list.items[index];
        if (front->translation.x != back->translation.x ||
            front->translation.y != back->translation.y) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                static_cast<std::uint32_t>(12U + index));
        }
    }
    output.cell_count = static_cast<std::uint16_t>(
        output.tab_list.items.size());
    return {};
}

[[nodiscard]] SettingsGfxError locate_panel(
    const Model& model,
    const PanelContract& contract,
    const SpriteView* expected_window_list,
    PanelContext& output) noexcept {
    output = {};
    std::size_t references{};
    std::uint16_t panel_id{};
    for (const auto& sprite : model.sprites()) {
        if (expected_window_list && &sprite != expected_window_list) continue;
        for (const auto* placement : sprite.placements) {
            if (!placement->has_name || placement->name != contract.name) continue;
            if (!placement->has_character) {
                return semantic_error(
                    SettingsGfxErrorCode::incompatible_structure,
                    contract.feature,
                    1);
            }
            ++references;
            panel_id = placement->character_id;
            output.window_list = &sprite;
        }
    }
    if (references != 1 || !output.window_list) {
        if (references == 0 && expected_window_list) {
            const bool exists_under_another_owner = std::any_of(
                model.sprites().begin(),
                model.sprites().end(),
                [&](const SpriteView& sprite) {
                    if (&sprite == expected_window_list) return false;
                    return std::any_of(
                        sprite.placements.begin(),
                        sprite.placements.end(),
                        [&](const GfxPlacementInfo* placement) {
                            return placement->has_name &&
                                placement->name == contract.name;
                        });
                });
            if (exists_under_another_owner) {
                return semantic_error(
                    SettingsGfxErrorCode::incompatible_structure,
                    contract.feature,
                    9);
            }
        }
        return semantic_error(references ? SettingsGfxErrorCode::ambiguous_structure
                                         : SettingsGfxErrorCode::structure_not_found,
            contract.feature, static_cast<std::uint32_t>(references));
    }
    output.panel = model.sprite(panel_id);
    if (!output.panel) {
        return semantic_error(
            SettingsGfxErrorCode::structure_not_found,
            contract.feature,
            2);
    }
    // These panels are single-frame libraries. Accepting a similarly named
    // multi-frame sprite (or one with an ambiguous/missing ShowFrame) would
    // make row insertion target an arbitrary timeline boundary.
    if (output.panel->info.frame_count != 1 || !model.show_frame(*output.panel)) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            contract.feature,
            8);
    }

    std::unordered_map<std::uint16_t, const GfxPlacementInfo*> indexed{};
    std::unordered_set<std::uint16_t> depths{};
    depths.reserve(output.panel->placements.size());
    for (const auto* placement : output.panel->placements) {
        if (!depths.emplace(placement->depth).second) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                10);
        }
    }
    for (const auto* placement : output.panel->placements) {
        if (!placement->has_name) continue;
        const auto index = item_index(placement->name);
        if (!index) continue;
        if (placement->tag_code != kTagPlaceObject2 ||
            placement->flags != 0x26U || placement->move ||
            !placement->has_character || !placement->has_matrix ||
            placement->translation.x != 0 || !indexed.emplace(*index, placement).second) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                3);
        }
    }
    if (indexed.size() < contract.minimum_rows) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            contract.feature,
            static_cast<std::uint32_t>(indexed.size()));
    }
    const auto maximum = std::max_element(indexed.begin(), indexed.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    if (maximum->first == std::numeric_limits<std::uint16_t>::max()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            contract.feature,
            7);
    }
    const auto count = static_cast<std::uint16_t>(maximum->first + 1);
    if (count != indexed.size()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            contract.feature,
            5);
    }
    output.items.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto it = indexed.find(index);
        if (it == indexed.end() ||
            it->second->translation.y != -4800 + index * 1000)
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                6);
        output.items.push_back(it->second);
    }

    if (contract.character_policy == PanelCharacterPolicy::uniform) {
        output.item_character = output.items.front()->character_id;
        if (std::any_of(
                output.items.begin(),
                output.items.end(),
                [&](const GfxPlacementInfo* item) {
                    return item->character_id != output.item_character;
                })) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                4);
        }
    } else {
        // Keyboard/Mouse and Graphics author six native rows followed by one
        // generic spare row. Existing third-party extensions must continue
        // using that spare's character definition, never the native-row
        // character that happens to be numerically or visually adjacent.
        const std::size_t spare = contract.minimum_rows - 1U;
        const std::uint16_t native_character =
            output.items.front()->character_id;
        if (std::any_of(
                output.items.begin(),
                output.items.begin() + static_cast<std::ptrdiff_t>(spare),
                [&](const GfxPlacementInfo* item) {
                    return item->character_id != native_character;
                })) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                4);
        }
        output.item_character = output.items[spare]->character_id;
        if (std::any_of(
                output.items.begin() + static_cast<std::ptrdiff_t>(spare),
                output.items.end(),
                [&](const GfxPlacementInfo* item) {
                    return item->character_id != output.item_character;
                })) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                11);
        }
    }
    output.row_count = count;
    return {};
}

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 24));
}
void write_u16(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
    out[offset] = static_cast<std::uint8_t>(value);
    out[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : out_(out) {}
    void write(std::uint32_t value, std::uint32_t count) {
        for (std::uint32_t bit = count; bit; --bit) {
            if (offset_ == 0) out_.push_back(0);
            out_.back() |= static_cast<std::uint8_t>(((value >> (bit - 1)) & 1U) << (7U - offset_));
            offset_ = static_cast<std::uint8_t>((offset_ + 1U) & 7U);
        }
    }
    void write_signed(std::int32_t value, std::uint32_t count) {
        const auto mask = count == 32 ? std::numeric_limits<std::uint32_t>::max() : ((1U << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }
    void align() noexcept { offset_ = 0; }
private:
    std::vector<std::uint8_t>& out_;
    std::uint8_t offset_{};
};

class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes, std::size_t offset = 0)
        : bytes_(bytes), bit_(offset * 8U) {}
    [[nodiscard]] bool read(std::size_t count, std::uint32_t& value) noexcept {
        if (count > 32 || bit_ + count > bytes_.size() * 8U) return false;
        value = 0;
        for (std::size_t index = 0; index < count; ++index, ++bit_) {
            value = (value << 1U) |
                ((bytes_[bit_ / 8U] >> (7U - (bit_ % 8U))) & 1U);
        }
        return true;
    }
    [[nodiscard]] bool read_signed(
        std::size_t count,
        std::int32_t& value) noexcept {
        std::uint32_t encoded{};
        if (count == 0U || !read(count, encoded)) return false;
        if (count < 32U &&
            (encoded & (std::uint32_t{1} << (count - 1U))) != 0U) {
            encoded |= ~((std::uint32_t{1} << count) - 1U);
        }
        value = static_cast<std::int32_t>(encoded);
        return true;
    }
    void align() noexcept { bit_ = (bit_ + 7U) & ~std::size_t{7U}; }
    [[nodiscard]] std::size_t offset() const noexcept { return bit_ / 8U; }
private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t bit_{};
};

struct MatrixFields {
    std::optional<std::int32_t> scale_x{};
    std::optional<std::int32_t> scale_y{};
    std::optional<std::int32_t> rotate_skew_0{};
    std::optional<std::int32_t> rotate_skew_1{};
    std::int32_t x{};
    std::int32_t y{};
    std::size_t end_offset{};
};

[[nodiscard]] std::uint32_t signed_bits(std::int32_t value) noexcept;

[[nodiscard]] bool decode_matrix(
    std::span<const std::uint8_t> body,
    std::size_t offset,
    MatrixFields& output) noexcept {
    output = {};
    BitReader reader(body, offset);
    std::uint32_t present{};
    std::uint32_t bits{};
    if (!reader.read(1, present)) return false;
    if (present != 0U) {
        std::int32_t x{};
        std::int32_t y{};
        if (!reader.read(5, bits) || bits == 0U || bits > 32U ||
            !reader.read_signed(bits, x) ||
            !reader.read_signed(bits, y)) {
            return false;
        }
        output.scale_x = x;
        output.scale_y = y;
    }
    if (!reader.read(1, present)) return false;
    if (present != 0U) {
        std::int32_t first{};
        std::int32_t second{};
        if (!reader.read(5, bits) || bits == 0U || bits > 32U ||
            !reader.read_signed(bits, first) ||
            !reader.read_signed(bits, second)) {
            return false;
        }
        output.rotate_skew_0 = first;
        output.rotate_skew_1 = second;
    }
    if (!reader.read(5, bits) || bits == 0U || bits > 32U ||
        !reader.read_signed(bits, output.x) ||
        !reader.read_signed(bits, output.y)) {
        return false;
    }
    reader.align();
    output.end_offset = reader.offset();
    return output.end_offset <= body.size();
}

void append_matrix(
    std::vector<std::uint8_t>& out,
    const MatrixFields& fields,
    std::int32_t x,
    std::int32_t y) {
    BitWriter writer(out);
    const bool has_scale = fields.scale_x && fields.scale_y;
    writer.write(has_scale ? 1U : 0U, 1);
    if (has_scale) {
        const auto bits = std::max(
            signed_bits(*fields.scale_x), signed_bits(*fields.scale_y));
        writer.write(bits, 5);
        writer.write_signed(*fields.scale_x, bits);
        writer.write_signed(*fields.scale_y, bits);
    }
    const bool has_rotate =
        fields.rotate_skew_0 && fields.rotate_skew_1;
    writer.write(has_rotate ? 1U : 0U, 1);
    if (has_rotate) {
        const auto bits = std::max(
            signed_bits(*fields.rotate_skew_0),
            signed_bits(*fields.rotate_skew_1));
        writer.write(bits, 5);
        writer.write_signed(*fields.rotate_skew_0, bits);
        writer.write_signed(*fields.rotate_skew_1, bits);
    }
    const auto translate_bits = std::max(signed_bits(x), signed_bits(y));
    writer.write(translate_bits, 5);
    writer.write_signed(x, translate_bits);
    writer.write_signed(y, translate_bits);
    writer.align();
}

[[nodiscard]] bool skip_matrix(std::span<const std::uint8_t> body, std::size_t& offset) {
    BitReader reader(body, offset);
    std::uint32_t present{}, bits{}, ignored{};
    if (!reader.read(1, present)) return false;
    if (present && (!reader.read(5, bits) || bits > 31 ||
        !reader.read(bits, ignored) || !reader.read(bits, ignored))) return false;
    if (!reader.read(1, present)) return false;
    if (present && (!reader.read(5, bits) || bits > 31 ||
        !reader.read(bits, ignored) || !reader.read(bits, ignored))) return false;
    if (!reader.read(5, bits) || bits > 31 ||
        !reader.read(bits, ignored) || !reader.read(bits, ignored)) return false;
    reader.align(); offset = reader.offset(); return offset <= body.size();
}

[[nodiscard]] bool skip_color(std::span<const std::uint8_t> body, std::size_t& offset) {
    BitReader reader(body, offset);
    std::uint32_t add{}, multiply{}, bits{}, ignored{};
    if (!reader.read(1, add) || !reader.read(1, multiply) ||
        !reader.read(4, bits) || bits > 31) return false;
    const auto terms = static_cast<std::size_t>((add ? 4 : 0) + (multiply ? 4 : 0));
    for (std::size_t index = 0; index < terms; ++index)
        if (!reader.read(bits, ignored)) return false;
    reader.align(); offset = reader.offset(); return offset <= body.size();
}

[[nodiscard]] std::uint32_t signed_bits(std::int32_t value) noexcept {
    for (std::uint32_t bits = 1; bits < 32; ++bits) {
        const auto min = -(std::int64_t{1} << (bits - 1));
        const auto max = (std::int64_t{1} << (bits - 1)) - 1;
        if (value >= min && value <= max) return bits;
    }
    return 32;
}

void append_matrix(
    std::vector<std::uint8_t>& out, std::int32_t x, std::int32_t y,
    std::optional<std::int32_t> scale_x = std::nullopt,
    std::optional<std::int32_t> scale_y = std::nullopt) {
    BitWriter writer(out);
    const bool has_scale = scale_x && scale_y;
    writer.write(has_scale ? 1U : 0U, 1);
    if (has_scale) {
        const auto bits = std::max(signed_bits(*scale_x), signed_bits(*scale_y));
        writer.write(bits, 5);
        writer.write_signed(*scale_x, bits);
        writer.write_signed(*scale_y, bits);
    }
    writer.write(0, 1); // rotate
    const auto bits = std::max(signed_bits(x), signed_bits(y));
    writer.write(bits, 5);
    writer.write_signed(x, bits);
    writer.write_signed(y, bits);
    writer.align();
}

void append_character_frame_color_transform(std::vector<std::uint8_t>& out) {
    BitWriter writer(out);
    writer.write(0, 1);
    writer.write(1, 1);
    writer.write(10, 4);
    for (const auto value : {225, 225, 225, 256}) writer.write_signed(value, 10);
    writer.align();
}

[[nodiscard]] std::vector<std::uint8_t> encode_tag(
    std::uint16_t code, std::span<const std::uint8_t> body,
    bool force_long = false) {
    std::vector<std::uint8_t> out{};
    if (!force_long && body.size() < 0x3FU) {
        append_u16(out, static_cast<std::uint16_t>((code << 6U) | body.size()));
    } else {
        append_u16(out, static_cast<std::uint16_t>((code << 6U) | 0x3FU));
        append_u32(out, static_cast<std::uint32_t>(body.size()));
    }
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

[[nodiscard]] GfxRawTag character_frame_external(std::uint16_t id) {
    GfxRawTag tag{.code = kTagDefineExternalImage2};
    append_u16(tag.body, id);
    append_u16(tag.body, 0);
    tag.body.push_back(13);
    tag.body.push_back(0);
    append_u16(tag.body, 428);
    append_u16(tag.body, 108);
    tag.body.push_back(static_cast<std::uint8_t>(kCharacterFrameExport.size()));
    tag.body.insert(tag.body.end(), kCharacterFrameExport.begin(), kCharacterFrameExport.end());
    tag.body.push_back(static_cast<std::uint8_t>(kCharacterFrameFile.size()));
    tag.body.insert(tag.body.end(), kCharacterFrameFile.begin(), kCharacterFrameFile.end());
    return tag;
}

[[nodiscard]] GfxRawTag character_frame_wrapper(
    std::uint16_t wrapper_id, std::uint16_t image_id) {
    std::vector<std::uint8_t> image_body{0x06U, 0x10U};
    append_u16(image_body, 1);
    append_u16(image_body, image_id);
    append_matrix(image_body, -4193, -1080);
    GfxRawTag tag{.code = kTagDefineSprite, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, wrapper_id);
    append_u16(tag.body, 1);
    for (const auto& child : {
            encode_tag(kTagPlaceObject3, image_body, true),
            encode_tag(kTagShowFrame, {}), encode_tag(kTagEnd, {})}) {
        tag.body.insert(tag.body.end(), child.begin(), child.end());
    }
    return tag;
}

[[nodiscard]] GfxRawTag character_frame_grid(std::uint16_t wrapper_id) {
    GfxRawTag tag{.code = kTagDefineScalingGrid};
    append_u16(tag.body, wrapper_id);
    BitWriter rectangle(tag.body);
    rectangle.write(9, 5);
    rectangle.write_signed(-80, 9);
    rectangle.write_signed(80, 9);
    rectangle.write_signed(-160, 9);
    rectangle.write_signed(180, 9);
    rectangle.align();
    return tag;
}

[[nodiscard]] GfxRawTag character_frame_placement(std::uint16_t wrapper_id) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x2EU);
    append_u16(tag.body, 2);
    append_u16(tag.body, wrapper_id);
    append_matrix(tag.body, -487, -320, 65536, 31457);
    append_character_frame_color_transform(tag.body);
    tag.body.insert(tag.body.end(), kCharacterFrame.begin(), kCharacterFrame.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag native_named_placement(
    std::uint16_t character, std::uint16_t depth, std::int32_t x,
    std::int32_t y, std::string_view name) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x26U);
    append_u16(tag.body, depth);
    append_u16(tag.body, character);
    BitWriter matrix(tag.body);
    matrix.write(0, 1);
    matrix.write(0, 1);
    matrix.write(15, 5);
    matrix.write(static_cast<std::uint32_t>(x) & 0x7FFFU, 15);
    matrix.write(static_cast<std::uint32_t>(y) & 0x7FFFU, 15);
    matrix.align();
    tag.body.insert(tag.body.end(), name.begin(), name.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag row_placement(
    std::uint16_t character,
    std::uint16_t index,
    std::uint16_t depth) {
    return native_named_placement(character, depth, 0,
        -4800 + static_cast<std::int32_t>(index) * 1000,
        "Item_" + std::to_string(index) + "_0");
}

[[nodiscard]] std::optional<GfxRawTag> raw_tag(
    const GfxDocument& document,
    GfxTagHandle handle);

[[nodiscard]] SettingsGfxError donor_row_placement(
    const GfxDocument& document,
    const Model& model,
    const PanelContext& panel,
    std::uint16_t index,
    std::uint16_t target_depth,
    SettingsGfxFeature feature,
    std::optional<GfxRawTag>& output) {
    output.reset();
    const std::string name = "Item_" + std::to_string(index) + "_0";
    const std::int32_t expected_y =
        -4800 + static_cast<std::int32_t>(index) * 1000;
    for (const auto& sprite : model.sprites()) {
        if (sprite.info.definition == panel.panel->info.definition) continue;
        for (const auto* placement : sprite.placements) {
            if (placement->tag_code != kTagPlaceObject2 ||
                placement->flags != 0x26U ||
                !placement->has_name || placement->name != name ||
                !placement->has_character ||
                placement->character_id != panel.item_character ||
                !placement->has_matrix || placement->translation.x != 0 ||
                placement->translation.y != expected_y) {
                continue;
            }
            const auto raw = raw_tag(document, placement->tag);
            if (!raw || raw->body.size() < 3U) {
                return semantic_error(
                    SettingsGfxErrorCode::invalid_document, feature, index);
            }
            output = *raw;
            // Match the established patcher contract: the first structurally
            // compatible authored row is the donor. Dictionary order is
            // deterministic, while later duplicate templates are unrelated
            // alternatives that must remain untouched.
            break;
        }
        if (output) break;
    }
    if (output) {
        write_u16(output->body, 1, target_depth);
    }
    return {};
}

[[nodiscard]] SettingsGfxError apply_one(
    GfxDocument& document, GfxEdit edit, SettingsGfxFeature feature) {
    const std::array edits{std::move(edit)};
    const auto error = document.apply(edits);
    return error.success() ? SettingsGfxError{} : document_error(feature, error);
}

[[nodiscard]] std::optional<GfxRawTag> raw_tag(
    const GfxDocument& document, GfxTagHandle handle) {
    auto result = document.copy_tag(handle);
    if (!result.success()) return std::nullopt;
    return std::move(result.tag);
}

struct ExternalImage {
    std::uint16_t id{};
    std::uint16_t id_type{};
    std::uint8_t format{};
    std::uint8_t reserved{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::string export_name{};
    std::string file_name{};
};

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
}

[[nodiscard]] bool length_string(
    std::span<const std::uint8_t> body, std::size_t& offset, std::string& out) {
    if (offset >= body.size()) return false;
    const auto count = body[offset++];
    if (count > body.size() - offset) return false;
    out.assign(reinterpret_cast<const char*>(body.data() + offset), count);
    offset += count;
    return true;
}

[[nodiscard]] bool parse_external(const GfxRawTag& tag, ExternalImage& out) {
    if (tag.code != kTagDefineExternalImage2 || tag.body.size() < 10) return false;
    out = {};
    out.id = read_u16(tag.body, 0);
    out.id_type = read_u16(tag.body, 2);
    out.format = tag.body[4];
    out.reserved = tag.body[5];
    out.width = read_u16(tag.body, 6);
    out.height = read_u16(tag.body, 8);
    std::size_t offset = 10;
    return length_string(tag.body, offset, out.export_name) &&
        length_string(tag.body, offset, out.file_name) && offset == tag.body.size();
}

struct Match { std::size_t count{}; std::size_t offset{}; };
[[nodiscard]] Match find_sequence(
    std::span<const std::uint8_t> bytes, std::span<const std::uint8_t> sequence) noexcept {
    Match result{};
    auto begin = bytes.begin();
    while (begin != bytes.end()) {
        const auto found = std::search(begin, bytes.end(), sequence.begin(), sequence.end());
        if (found == bytes.end()) break;
        ++result.count;
        result.offset = static_cast<std::size_t>(std::distance(bytes.begin(), found));
        begin = std::next(found);
    }
    return result;
}

enum class PlaceholderColor { invalid, gray, red };
[[nodiscard]] PlaceholderColor placeholder_color(
    const GfxDocument& document, std::uint16_t id,
    GfxTagHandle* definition = nullptr) {
    const auto info = top_level_character_definition(document, id);
    if (!info || info->tag_code != kTagDefineEditText) return PlaceholderColor::invalid;
    const auto raw = raw_tag(document, info->tag);
    if (!raw) return PlaceholderColor::invalid;
    if (definition) *definition = info->tag;
    constexpr std::array<std::uint8_t, 4> gray{0x50, 0x50, 0x50, 0xFF};
    constexpr std::array<std::uint8_t, 4> red{0xFF, 0x00, 0x00, 0xFF};
    constexpr std::array<std::uint8_t, 7> gray_html{'#','5','0','5','0','5','0'};
    constexpr std::array<std::uint8_t, 7> red_html{'#','F','F','0','0','0','0'};
    const auto g = find_sequence(raw->body, gray);
    const auto r = find_sequence(raw->body, red);
    const auto gh = find_sequence(raw->body, gray_html);
    const auto rh = find_sequence(raw->body, red_html);
    if (g.count == 1 && gh.count == 1 && !r.count && !rh.count) return PlaceholderColor::gray;
    if (r.count == 1 && rh.count == 1 && !g.count && !gh.count) return PlaceholderColor::red;
    return PlaceholderColor::invalid;
}

[[nodiscard]] std::size_t named_count(const SpriteView& sprite, std::string_view name) {
    return static_cast<std::size_t>(std::count_if(sprite.placements.begin(), sprite.placements.end(),
        [name](const auto* placement) { return placement->has_name && placement->name == name; }));
}

[[nodiscard]] bool text_child(
    const GfxPlacementInfo& placement, std::uint16_t depth,
    std::int32_t x, std::int32_t y) noexcept {
    return placement.flags == 0x26U && placement.has_character && placement.has_matrix &&
        placement.has_name && placement.depth == depth &&
        placement.translation.x == x && placement.translation.y == y;
}

[[nodiscard]] SettingsGfxError locate_text_input(
    const Model& model, TextInputContext& output) {
    output = {};
    std::size_t signatures{};
    for (const auto& sprite : model.sprites()) {
        const auto values = named_count(sprite, kValueText);
        const auto empties = named_count(sprite, kEmptyText);
        const auto captions = named_count(sprite, kCaption);
        const auto cursors = named_count(sprite, kCursor);
        if (!values || !empties || !captions || !cursors) continue;
        if (++signatures != 1 || values != 1 || empties != 1 || captions != 1 || cursors != 1)
            return semantic_error(SettingsGfxErrorCode::ambiguous_structure, SettingsGfxFeature::text_input, 1);
        const auto* value = model.unique(sprite, kValueText);
        const auto* empty = model.unique(sprite, kEmptyText);
        const auto* caption = model.unique(sprite, kCaption);
        const auto* cursor = model.unique(sprite, kCursor);
        if (!value || !empty || !caption || !cursor || sprite.info.frame_count != 1 ||
            !text_child(*value, 3, -4360, -640) || !text_child(*empty, 4, -4360, -640) ||
            !text_child(*cursor, 7, 0, 0) || caption->flags != 0x26U ||
            !caption->has_character || !caption->has_matrix || !caption->has_name ||
            caption->depth != 5 || caption->translation.y != 0 ||
            (caption->translation.x != -9520 && caption->translation.x != -9120) ||
            !model.show_frame(sprite)) {
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::text_input, 2);
        }
        const auto color = placeholder_color(model.document(), empty->character_id);
        if (color != PlaceholderColor::gray && color != PlaceholderColor::red)
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::text_input, 3);
        output = {&sprite, value, empty, caption, cursor,
            caption->translation.x == -9520 ? SettingsGfxHost::game_options
                                            : SettingsGfxHost::advanced_settings};
    }
    if (!output.sprite) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::text_input);
    return {};
}

[[nodiscard]] SettingsGfxError locate_widgets(
    const Model& model, const TextInputContext& text, WidgetContext& output) {
    output = {};
    output.host = text.host;
    std::unordered_set<std::uint16_t> seen_buttons{};
    seen_buttons.reserve(kMaximumWidgetHosts);
    for (const auto& sprite : model.sprites()) {
        const auto* input = model.unique(sprite, kTextInput);
        if (!input || !input->has_character || input->character_id != text.sprite->info.character_id) continue;
        const auto* button = model.unique(sprite, kButton);
        const auto* combo = model.unique(sprite, kComboBox);
        const auto* slider = model.unique(sprite, kSlider);
        if (!button || !combo || !slider || !button->has_character ||
            !combo->has_character || !slider->has_character || named_count(sprite, kColorPicker) > 1)
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::color_picker, 1);
        const auto* button_sprite = model.sprite(button->character_id);
        if (!button_sprite) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 1);
        if (button_sprite->info.frame_count == 0 ||
            !model.first_show_frame(*button_sprite)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::button_label,
                8);
        }
        const auto* frame = model.at_depth(*button_sprite, 3);
        const auto* cursor = model.unique(*button_sprite, kCursor);
        const auto* value = model.at_depth(*button_sprite, 10);
        const auto* label = model.at_depth(*button_sprite, 12);
        if (sprite.info.frame_count != 1 || !model.show_frame(sprite) ||
            !cursor || cursor->depth != 6 || !value ||
            !value->has_name || (label && !label->has_name) ||
            (value->name != "Text_0" && value->name != "Text_1") ||
            (label && label->name != "Text_0" && label->name != "Text_1") ||
            (label && value->name == label->name) ||
            (frame && frame->has_name && frame->name != kButtonFrame))
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::button_label, 2);
        if (output.hosts.size() == kMaximumWidgetHosts) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                static_cast<std::uint32_t>(kMaximumWidgetHosts + 1U));
        }
        output.hosts.push_back({sprite.info.character_id, button_sprite->info.character_id});
        if (seen_buttons.emplace(button_sprite->info.character_id).second)
            output.buttons.push_back(button_sprite->info.character_id);
    }
    if (output.hosts.empty()) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::color_picker, 2);
    std::sort(output.hosts.begin(), output.hosts.end(), [](const auto& a, const auto& b) {
        return std::tie(a.container, a.button) < std::tie(b.container, b.button); });
    std::sort(output.buttons.begin(), output.buttons.end());
    return {};
}

[[nodiscard]] const GfxPlacementInfo* tab_cell_donor(
    const TabLayerContext& layer) noexcept {
    std::unordered_map<std::uint16_t, std::size_t> counts{};
    counts.reserve(layer.items.size());
    for (const auto* item : layer.items) ++counts[item->character_id];

    std::uint16_t selected_character{};
    std::size_t selected_count{};
    bool tied{};
    for (const auto& [character, count] : counts) {
        if (count > selected_count) {
            selected_character = character;
            selected_count = count;
            tied = false;
        } else if (count == selected_count) {
            tied = true;
        }
    }
    if (selected_count == 0U || tied) return nullptr;
    const auto found = std::find_if(
        layer.items.rbegin(), layer.items.rend(),
        [&](const GfxPlacementInfo* item) {
            return item->character_id == selected_character;
        });
    return found == layer.items.rend() ? nullptr : *found;
}

[[nodiscard]] std::optional<GfxRawTag> cloned_tab_cell(
    const GfxDocument& document,
    const GfxPlacementInfo& donor,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    auto raw = raw_tag(document, donor.tag);
    if (!raw || raw->code != kTagPlaceObject2 || raw->body.size() < 6U ||
        (raw->body[0] != 0x26U && raw->body[0] != 0x2EU)) {
        return std::nullopt;
    }

    MatrixFields matrix{};
    if (!decode_matrix(raw->body, 5U, matrix)) return std::nullopt;
    const auto representable = [](std::int32_t value) noexcept {
        // MATRIX encodes each signed-field width in five bits. Thirty-one is
        // therefore the largest representable width; writing 32 would encode
        // a zero-width field and shift every following optional field.
        return signed_bits(value) <= 31U;
    };
    if (!representable(x) || !representable(y) ||
        (matrix.scale_x && !representable(*matrix.scale_x)) ||
        (matrix.scale_y && !representable(*matrix.scale_y)) ||
        (matrix.rotate_skew_0 &&
            !representable(*matrix.rotate_skew_0)) ||
        (matrix.rotate_skew_1 &&
            !representable(*matrix.rotate_skew_1))) {
        return std::nullopt;
    }
    std::size_t name_offset = matrix.end_offset;
    if ((raw->body[0] & 0x08U) != 0U &&
        !skip_color(raw->body, name_offset)) {
        return std::nullopt;
    }
    if (name_offset >= raw->body.size()) return std::nullopt;
    const auto name_end = std::find(
        raw->body.begin() + static_cast<std::ptrdiff_t>(name_offset),
        raw->body.end(),
        std::uint8_t{0});
    if (name_end == raw->body.end()) return std::nullopt;
    const auto suffix_offset = static_cast<std::size_t>(
        std::distance(raw->body.begin(), name_end)) + 1U;

    std::vector<std::uint8_t> body{};
    body.reserve(raw->body.size() + name.size() + 16U);
    body.push_back(raw->body[0]);
    append_u16(body, depth);
    // Preserve the selected generic donor's character definition.
    body.insert(body.end(), raw->body.begin() + 3, raw->body.begin() + 5);
    append_matrix(body, matrix, x, y);
    body.insert(
        body.end(),
        raw->body.begin() + static_cast<std::ptrdiff_t>(matrix.end_offset),
        raw->body.begin() + static_cast<std::ptrdiff_t>(name_offset));
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);
    body.insert(
        body.end(),
        raw->body.begin() + static_cast<std::ptrdiff_t>(suffix_offset),
        raw->body.end());
    raw->body = std::move(body);
    return raw;
}

[[nodiscard]] std::optional<std::uint16_t> descending_tab_depth_stride(
    const TabLayerContext& layer) noexcept {
    if (layer.items.size() < 2U ||
        layer.items[0]->depth <= layer.items[1]->depth) {
        return std::nullopt;
    }
    const auto stride = static_cast<std::uint16_t>(
        layer.items[0]->depth - layer.items[1]->depth);
    if (stride == 0U) return std::nullopt;
    for (std::size_t index = 1U; index < layer.items.size(); ++index) {
        if (layer.items[index - 1U]->depth <= layer.items[index]->depth ||
            layer.items[index - 1U]->depth - layer.items[index]->depth !=
                stride) {
            return std::nullopt;
        }
    }
    return stride;
}

[[nodiscard]] std::optional<std::int32_t> uniform_tab_x_stride(
    const TabLayerContext& layer) noexcept {
    if (layer.items.size() < 2U) return std::nullopt;
    const std::int64_t stride =
        static_cast<std::int64_t>(layer.items[1]->translation.x) -
        layer.items[0]->translation.x;
    if (stride <= 0 || stride > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    for (std::size_t index = 2U; index < layer.items.size(); ++index) {
        if (static_cast<std::int64_t>(layer.items[index]->translation.x) -
                layer.items[index - 1U]->translation.x != stride) {
            return std::nullopt;
        }
    }
    return static_cast<std::int32_t>(stride);
}

[[nodiscard]] std::optional<std::uint16_t> appended_back_tab_depth_stride(
    const TabLayerContext& layer,
    std::size_t canonical_cells) noexcept {
    if (canonical_cells < 2U || canonical_cells > layer.items.size() ||
        layer.items[0]->depth <= layer.items[1]->depth) {
        return std::nullopt;
    }
    const auto stride = static_cast<std::uint16_t>(
        layer.items[0]->depth - layer.items[1]->depth);
    if (stride == 0U) return std::nullopt;
    for (std::size_t index = 1U; index < canonical_cells; ++index) {
        if (layer.items[index - 1U]->depth <= layer.items[index]->depth ||
            layer.items[index - 1U]->depth - layer.items[index]->depth !=
                stride) {
            return std::nullopt;
        }
    }
    for (std::size_t index = canonical_cells;
         index < layer.items.size();
         ++index) {
        const std::uint32_t expected =
            static_cast<std::uint32_t>(layer.items[0]->depth) +
            static_cast<std::uint32_t>(stride) *
                static_cast<std::uint32_t>(index - canonical_cells + 1U);
        if (expected > std::numeric_limits<std::uint16_t>::max() ||
            layer.items[index]->depth != expected) {
            return std::nullopt;
        }
    }
    return stride;
}

[[nodiscard]] bool exact_tab_growth_timeline(
    const Model& model,
    const TabStripContext& strip,
    std::uint16_t front_stride,
    std::uint16_t& back_stride,
    std::size_t& canonical_cells) noexcept {
    const auto* left = model.unique(*strip.tab_list.sprite, kLeftArrow);
    const auto* right = model.unique(*strip.tab_list.sprite, kRightArrow);
    const auto* text = model.unique(*strip.tab_list.sprite, kValueText);
    if (left == nullptr || right == nullptr || text == nullptr ||
        left->tag_code != kTagPlaceObject2 || left->depth != 1U ||
        right->tag_code != kTagPlaceObject2 || right->depth != 3U ||
        text->tag_code != kTagPlaceObject3 ||
        strip.tab_list.items.back()->depth != 5U ||
        front_stride != 6U ||
        text->depth != static_cast<std::uint32_t>(
            strip.tab_list.items.front()->depth) + front_stride ||
        strip.tab_list.sprite->placements.size() !=
            strip.tab_list.items.size() + 3U ||
        strip.back_tab_list.sprite->placements.size() !=
            strip.back_tab_list.items.size() + 1U ||
        strip.tab_list.sprite->tags.size() !=
            strip.tab_list.items.size() + 5U ||
        strip.back_tab_list.sprite->tags.size() !=
            strip.back_tab_list.items.size() + 3U) {
        return false;
    }

    const auto& front_tags = strip.tab_list.sprite->tags;
    const auto& back_tags = strip.back_tab_list.sprite->tags;
    const auto text_tag = std::find_if(
        front_tags.begin(), front_tags.end(),
        [text](const GfxTagInfo* tag) {
            return tag->handle == text->tag;
        });
    if (text_tag == front_tags.end()) return false;
    const auto text_ordinal = static_cast<std::size_t>(
        std::distance(front_tags.begin(), text_tag));
    if (text_ordinal < 2U) return false;
    canonical_cells = text_ordinal - 2U;
    if (canonical_cells < kMinimumAuthoredTabCells ||
        canonical_cells > strip.tab_list.items.size()) {
        return false;
    }
    const auto validated_back_stride = appended_back_tab_depth_stride(
        strip.back_tab_list, canonical_cells);
    if (!validated_back_stride || *validated_back_stride != 4U ||
        strip.back_tab_list.items[canonical_cells - 1U]->depth != 2U) {
        return false;
    }
    back_stride = *validated_back_stride;

    if (front_tags[0]->handle != left->tag ||
        front_tags[1]->handle != right->tag ||
        front_tags[front_tags.size() - 2U]->code != kTagShowFrame ||
        front_tags.back()->code != kTagEnd ||
        back_tags[back_tags.size() - 2U]->code != kTagShowFrame ||
        back_tags.back()->code != kTagEnd) {
        return false;
    }

    const auto back_background = std::find_if(
        strip.back_tab_list.sprite->placements.begin(),
        strip.back_tab_list.sprite->placements.end(),
        [](const GfxPlacementInfo* placement) {
            return placement->depth == 1U;
        });
    if (back_background == strip.back_tab_list.sprite->placements.end() ||
        (*back_background)->has_name ||
        back_tags[0]->handle != (*back_background)->tag) {
        return false;
    }

    // Canonical movies place every cell in reverse logical order before
    // Text_0. Movies previously expanded by ERNativeUI retain that canonical
    // prefix and append the generated logical tail after Text_0. Both forms
    // are exact, deterministic products of the supported authoring layouts.
    for (std::size_t ordinal = 0U; ordinal < canonical_cells; ++ordinal) {
        const std::size_t reverse = canonical_cells - 1U - ordinal;
        if (front_tags[ordinal + 2U]->handle !=
                strip.tab_list.items[reverse]->tag ||
            back_tags[ordinal + 1U]->handle !=
                strip.back_tab_list.items[reverse]->tag) {
            return false;
        }
    }
    for (std::size_t index = canonical_cells;
         index < strip.tab_list.items.size();
         ++index) {
        const std::size_t tail = index - canonical_cells;
        if (front_tags[text_ordinal + 1U + tail]->handle !=
                strip.tab_list.items[index]->tag ||
            back_tags[canonical_cells + 1U + tail]->handle !=
                strip.back_tab_list.items[index]->tag) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<GfxRawTag> placement_with_depth(
    const GfxDocument& document,
    const GfxPlacementInfo& placement,
    std::uint16_t depth) {
    auto raw = raw_tag(document, placement.tag);
    if (!raw) return std::nullopt;
    std::size_t offset{};
    if (placement.tag_code == kTagPlaceObject2) {
        offset = 1U;
    } else if (placement.tag_code == kTagPlaceObject3) {
        offset = 2U;
    } else {
        return std::nullopt;
    }
    if (raw->code != placement.tag_code ||
        raw->body.size() < offset + 2U) {
        return std::nullopt;
    }
    write_u16(raw->body, offset, depth);
    return raw;
}

[[nodiscard]] std::optional<GfxRawTag> place_object2_with_translation_x(
    const GfxDocument& document,
    const GfxPlacementInfo& placement,
    std::int32_t x) {
    auto raw = raw_tag(document, placement.tag);
    if (!raw || raw->code != kTagPlaceObject2 ||
        placement.tag_code != kTagPlaceObject2 || placement.move ||
        !placement.has_character || !placement.has_matrix ||
        raw->body.size() < 6U || raw->body[0] != placement.flags ||
        (raw->body[0] & 0x06U) != 0x06U) {
        return std::nullopt;
    }

    // PlaceObject2 stores Flags, Depth and CharacterId before MATRIX when
    // both HasCharacter and HasMatrix are set. Rebuild only MATRIX so every
    // authored field after it (including the instance name) remains byte-for-
    // byte unchanged.
    constexpr std::size_t matrix_offset = 5U;
    MatrixFields matrix{};
    if (!decode_matrix(raw->body, matrix_offset, matrix) ||
        matrix.x != placement.translation.x ||
        matrix.y != placement.translation.y) {
        return std::nullopt;
    }
    const auto representable = [](std::int32_t value) noexcept {
        return signed_bits(value) <= 31U;
    };
    if (!representable(x) || !representable(matrix.y) ||
        (matrix.scale_x && !representable(*matrix.scale_x)) ||
        (matrix.scale_y && !representable(*matrix.scale_y)) ||
        (matrix.rotate_skew_0 && !representable(*matrix.rotate_skew_0)) ||
        (matrix.rotate_skew_1 && !representable(*matrix.rotate_skew_1))) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> body{};
    body.reserve(raw->body.size() + 8U);
    body.insert(
        body.end(), raw->body.begin(),
        raw->body.begin() + static_cast<std::ptrdiff_t>(matrix_offset));
    append_matrix(body, matrix, x, matrix.y);
    body.insert(
        body.end(),
        raw->body.begin() + static_cast<std::ptrdiff_t>(matrix.end_offset),
        raw->body.end());
    raw->body = std::move(body);
    return raw;
}

[[nodiscard]] std::optional<std::int32_t> appended_tab_cell_x(
    const TabLayerContext& layer,
    std::int32_t stride,
    std::size_t offset) noexcept {
    if (layer.items.empty() || stride <= 0 || offset == 0U) {
        return std::nullopt;
    }
    const std::int64_t result =
        static_cast<std::int64_t>(layer.items.back()->translation.x) +
        static_cast<std::int64_t>(stride) *
            static_cast<std::int64_t>(offset);
    if (result < std::numeric_limits<std::int32_t>::min() ||
        result > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(result);
}

[[nodiscard]] SettingsGfxError grow_tab_strip(
    GfxDocument& document,
    std::uint16_t required_slots) {
    if (required_slots > kMaximumRequestedTabSlots) {
        return semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::tab_slots,
            required_slots);
    }
    const auto required_cells = static_cast<std::uint16_t>(required_slots + 1U);
    Model model(document);
    TabStripContext strip{};
    if (auto error = locate_tab_strip(model, strip); !error.success()) {
        return error;
    }
    // A third-party movie that already exposes enough cells is outside this
    // growth operation. Preserve it byte-for-byte even when its own arrow or
    // spacing contract differs from the exact timeline ERNativeUI needs in
    // order to author new cells safely.
    if (strip.cell_count >= required_cells) return {};

    const auto front_x_stride = uniform_tab_x_stride(strip.tab_list);
    const auto back_x_stride = uniform_tab_x_stride(strip.back_tab_list);
    if (!front_x_stride || !back_x_stride ||
        *front_x_stride != *back_x_stride) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            306);
    }
    const auto missing_cells = static_cast<std::size_t>(
        required_cells - strip.cell_count);
    const auto* front_donor = tab_cell_donor(strip.tab_list);
    const auto* back_donor = tab_cell_donor(strip.back_tab_list);
    const auto front_stride = descending_tab_depth_stride(strip.tab_list);
    const auto* front_frame = model.show_frame(*strip.tab_list.sprite);
    const auto* back_frame = model.show_frame(*strip.back_tab_list.sprite);
    std::uint16_t back_stride{};
    std::size_t canonical_cells{};
    if (front_donor == nullptr || back_donor == nullptr ||
        !front_stride || front_frame == nullptr || back_frame == nullptr ||
        !exact_tab_growth_timeline(
            model, strip, *front_stride, back_stride, canonical_cells)) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            300);
    }

    // The real movie does not allocate tab cells at max(depth)+1. BackTabList
    // extends its four-depth lattice above its current maximum. TabList keeps
    // the newest trailing cell at depth 5 and shifts its existing cell/text
    // block once by the complete missing span. Planning every cell from this
    // one immutable model is what makes a two-cell expansion both valid and
    // atomic.
    const std::uint16_t front_depth = strip.tab_list.items.back()->depth;
    const auto back_maximum = std::max_element(
        strip.back_tab_list.items.begin(),
        strip.back_tab_list.items.end(),
        [](const GfxPlacementInfo* left, const GfxPlacementInfo* right) {
            return left->depth < right->depth;
        });
    const std::uint32_t front_shift_wide =
        static_cast<std::uint32_t>(*front_stride) *
        static_cast<std::uint32_t>(missing_cells);
    if (front_depth == 0U ||
        back_maximum == strip.back_tab_list.items.end() ||
        front_shift_wide > std::numeric_limits<std::uint16_t>::max()) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots,
            302);
    }

    std::vector<GfxEdit> edits{};
    edits.reserve(
        strip.tab_list.sprite->placements.size() + missing_cells * 2U + 1U);
    for (const auto* placement : strip.tab_list.sprite->placements) {
        if (placement->depth < front_depth) continue;
        const std::uint32_t shifted =
            static_cast<std::uint32_t>(placement->depth) + front_shift_wide;
        if (shifted > std::numeric_limits<std::uint16_t>::max()) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                303);
        }
        auto replacement = placement_with_depth(
            document,
            *placement,
            static_cast<std::uint16_t>(shifted));
        if (!replacement) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                304);
        }
        edits.push_back(GfxEdit::replace_body(
            placement->tag,
            std::move(replacement->body)));
    }

    // The stock movie authors one more Item_0_N cell than its eight native
    // records. That trailing cell is the ninth-slot spare, and RightArrow is
    // already positioned beyond it. Consuming that spare therefore needs no
    // arrow movement. Every cell added beyond the spare extends the visible
    // strip and moves the RB prompt by the same proven x lattice. Computing
    // this from the final cell count makes direct 8 -> 10 and staged
    // 8 -> 9 -> 10 transformations byte-identical and prevents a repeated
    // request from shifting the prompt twice.
    constexpr std::size_t first_cell_beyond_stock_spare =
        static_cast<std::size_t>(kMinimumAuthoredTabCells) + 1U;
    const std::size_t covered_cells = std::max(
        static_cast<std::size_t>(strip.cell_count),
        first_cell_beyond_stock_spare);
    const std::size_t final_cells = static_cast<std::size_t>(required_cells);
    const std::size_t right_shift_cells =
        final_cells > covered_cells ? final_cells - covered_cells : 0U;
    if (right_shift_cells != 0U) {
        const auto* right = model.unique(*strip.tab_list.sprite, kRightArrow);
        const std::int64_t shifted_x =
            static_cast<std::int64_t>(right->translation.x) +
            static_cast<std::int64_t>(*front_x_stride) *
                static_cast<std::int64_t>(right_shift_cells);
        if (shifted_x < std::numeric_limits<std::int32_t>::min() ||
            shifted_x > std::numeric_limits<std::int32_t>::max()) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                305);
        }
        auto replacement = place_object2_with_translation_x(
            document, *right, static_cast<std::int32_t>(shifted_x));
        if (!replacement) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                305);
        }
        edits.push_back(GfxEdit::replace_body(
            right->tag, std::move(replacement->body)));
    }

    for (std::size_t offset = 1U; offset <= missing_cells; ++offset) {
        const std::uint16_t logical_index = static_cast<std::uint16_t>(
            static_cast<std::size_t>(strip.cell_count) + offset - 1U);
        const auto front_x = appended_tab_cell_x(
            strip.tab_list, *front_x_stride, offset);
        const auto back_x = appended_tab_cell_x(
            strip.back_tab_list, *back_x_stride, offset);
        const std::uint32_t front_depth_wide =
            static_cast<std::uint32_t>(front_depth) +
            static_cast<std::uint32_t>(*front_stride) *
                static_cast<std::uint32_t>(missing_cells - offset);
        const std::uint32_t back_depth_wide =
            static_cast<std::uint32_t>((*back_maximum)->depth) +
            static_cast<std::uint32_t>(back_stride) *
                static_cast<std::uint32_t>(offset);
        if (!front_x || !back_x || *front_x != *back_x ||
            front_depth_wide > std::numeric_limits<std::uint16_t>::max() ||
            back_depth_wide > std::numeric_limits<std::uint16_t>::max() ||
            model.any_placement_at_depth(
                *strip.back_tab_list.sprite,
                static_cast<std::uint16_t>(back_depth_wide))) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                302);
        }
        const std::string name = "Item_0_" + std::to_string(logical_index);
        auto front = cloned_tab_cell(
            document,
            *front_donor,
            static_cast<std::uint16_t>(front_depth_wide),
            *front_x,
            front_donor->translation.y,
            name);
        auto back = cloned_tab_cell(
            document,
            *back_donor,
            static_cast<std::uint16_t>(back_depth_wide),
            *back_x,
            back_donor->translation.y,
            name);
        if (!front || !back) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::tab_slots,
                301);
        }
        edits.push_back(GfxEdit::insert_before(
            front_frame->handle, std::move(*front)));
        edits.push_back(GfxEdit::insert_before(
            back_frame->handle, std::move(*back)));
    }

    const auto error = document.apply(edits);
    if (!error.success()) {
        return document_error(SettingsGfxFeature::tab_slots, error);
    }
    return {};
}

[[nodiscard]] SettingsGfxError grow_panel(
    GfxDocument& document,
    const PanelContract& contract,
    std::uint16_t expected_window_list_id,
    std::uint16_t required) {
    if (required > kMaximumRows)
        return semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            contract.feature,
            required);
    for (;;) {
        Model model(document);
        PanelContext panel{};
        // Reacquire the WindowList by the stable character ID after each
        // document edit. Model and every view into it are revision-local.
        const SpriteView* window_list = expected_window_list_id != 0
            ? model.sprite(expected_window_list_id)
            : nullptr;
        if (expected_window_list_id != 0 && !window_list) {
            return semantic_error(
                SettingsGfxErrorCode::structure_not_found,
                contract.feature,
                12);
        }
        if (auto error = locate_panel(
                model, contract, window_list, panel);
            !error.success()) {
            return error;
        }
        if (panel.row_count >= required) return {};

        std::uint32_t maximum_depth{};
        for (const auto* placement : panel.panel->placements) {
            maximum_depth = std::max<std::uint32_t>(
                maximum_depth, placement->depth);
        }
        std::uint32_t target_depth = maximum_depth + 1U;
        if ((target_depth & 1U) == 0U) ++target_depth;
        if (target_depth > std::numeric_limits<std::uint16_t>::max()) {
            return semantic_error(
                SettingsGfxErrorCode::resource_conflict,
                contract.feature,
                maximum_depth);
        }
        const auto* frame = model.show_frame(*panel.panel);
        if (!frame) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                contract.feature,
                7);
        }
        std::optional<GfxRawTag> donor{};
        if (auto error = donor_row_placement(
                document,
                model,
                panel,
                panel.row_count,
                static_cast<std::uint16_t>(target_depth),
                contract.feature,
                donor);
            !error.success()) {
            return error;
        }
        auto edit = GfxEdit::insert_before(frame->handle,
            donor ? std::move(*donor)
                  : row_placement(
                        panel.item_character,
                        panel.row_count,
                        static_cast<std::uint16_t>(target_depth)));
        if (auto error = apply_one(
                document, std::move(edit), contract.feature);
            !error.success()) {
            return error;
        }
    }
}

[[nodiscard]] bool raw_equal(const GfxRawTag& left, const GfxRawTag& right) noexcept {
    const auto concrete_header = [](const GfxRawTag& tag) noexcept {
        if (tag.header_form != GfxTagHeaderForm::automatic) {
            return tag.header_form;
        }
        return tag.body.size() < 0x3FU
            ? GfxTagHeaderForm::short_form
            : GfxTagHeaderForm::long_form;
    };
    return left.code == right.code && left.body == right.body &&
        concrete_header(left) == concrete_header(right);
}

[[nodiscard]] std::optional<ExternalImage> find_external(
    const GfxDocument& document, std::string_view export_name,
    std::string_view file_name, std::uint16_t width, std::uint16_t height,
    bool& conflict) {
    conflict = false;
    std::optional<ExternalImage> found{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite || tag.code != kTagDefineExternalImage2) continue;
        const auto raw = raw_tag(document, tag.handle);
        ExternalImage image{};
        if (!raw || !parse_external(*raw, image)) { conflict = true; return std::nullopt; }
        const bool related = image.export_name == export_name || image.file_name == file_name;
        if (!related) continue;
        if (found || image.export_name != export_name || image.file_name != file_name ||
            image.id_type != 0 || image.format != 13 || image.reserved != 0 ||
            image.width != width || image.height != height) {
            conflict = true;
            return std::nullopt;
        }
        found = image;
    }
    return found;
}

struct CharacterFrameResource { std::uint16_t image{}; std::uint16_t wrapper{}; };

[[nodiscard]] bool character_frame_matches(
    const GfxDocument& document, const TextInputContext& text,
    CharacterFrameResource* resource = nullptr) {
    Model model(document);
    const auto* sprite = model.sprite(text.sprite->info.character_id);
    if (!sprite) return false;
    const auto* frame = model.unique(*sprite, kCharacterFrame);
    const auto* empty = model.unique(*sprite, kEmptyText);
    if (!frame || !empty || !frame->has_character || !empty->has_character) return false;
    if (static_cast<std::size_t>(std::count_if(
            sprite->placements.begin(), sprite->placements.end(),
            [](const GfxPlacementInfo* placement) {
                return placement->depth == 2;
            })) != 1U) {
        return false;
    }
    const auto placement = raw_tag(document, frame->tag);
    if (!placement || !raw_equal(*placement, character_frame_placement(frame->character_id))) return false;
    bool conflict{};
    const auto image = find_external(document, kCharacterFrameExport, kCharacterFrameFile, 428, 108, conflict);
    if (conflict || !image) return false;
    const auto* wrapper_info = model.sprite(frame->character_id);
    const auto image_info = top_level_character_definition(document, image->id);
    if (!wrapper_info) return false;
    const auto wrapper = raw_tag(document, wrapper_info->info.definition);
    if (!wrapper || !raw_equal(*wrapper, character_frame_wrapper(frame->character_id, image->id))) return false;
    bool grid_found{};
    std::optional<std::size_t> grid_ordinal{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
        const auto raw = raw_tag(document, tag.handle);
        if (!raw || raw->body.size() < 2 || read_u16(raw->body, 0) != frame->character_id) continue;
        if (grid_found || !raw_equal(*raw, character_frame_grid(frame->character_id))) return false;
        grid_found = true;
        grid_ordinal = tag.ordinal;
    }
    if (!grid_found || placeholder_color(document, empty->character_id) != PlaceholderColor::red) return false;
    const auto wrapper_tag = top_level_tag(
        document, wrapper_info->info.definition);
    const auto image_tag = image_info ? document.tag(image_info->tag) : std::nullopt;
    const auto user_tag = top_level_tag(document, text.sprite->info.definition);
    if (!wrapper_tag || !image_tag || !user_tag || !grid_ordinal ||
        image_tag->ordinal >= wrapper_tag->ordinal ||
        wrapper_tag->ordinal >= *grid_ordinal ||
        *grid_ordinal >= user_tag->ordinal) return false;
    if (resource) *resource = {image->id, frame->character_id};
    return true;
}

[[nodiscard]] SettingsGfxError replace_top_level_before(
    GfxDocument& document,
    GfxTagHandle target,
    std::span<const GfxTagHandle> erase,
    std::span<GfxRawTag> insert,
    SettingsGfxFeature feature) {
    std::vector<GfxEdit> edits{};
    edits.reserve(erase.size() + insert.size());
    for (auto& tag : insert) {
        edits.push_back(GfxEdit::insert_before(target, std::move(tag)));
    }
    for (const GfxTagHandle handle : erase) {
        edits.push_back(GfxEdit::erase(handle));
    }
    const auto error = document.apply(edits);
    return error.success() ? SettingsGfxError{} : document_error(feature, error);
}

[[nodiscard]] SettingsGfxError ensure_character_frame_resource(
    GfxDocument& document, GfxTagHandle first_user,
    SettingsGfxFeature feature,
    std::optional<std::uint16_t> required_wrapper,
    CharacterFrameResource& output, bool& changed) {
    changed = false;
    bool conflict{};
    const auto existing = find_external(
        document, kCharacterFrameExport, kCharacterFrameFile, 428, 108, conflict);
    if (conflict) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict, feature, 1);
    }

    const auto user_tag = top_level_tag(document, first_user);
    if (!user_tag) {
        return semantic_error(
            SettingsGfxErrorCode::structure_not_found, feature, 1);
    }

    std::optional<std::uint16_t> wrapper{};
    if (existing) {
        const Model model(document);
        if (required_wrapper) {
            const auto* sprite = model.sprite(*required_wrapper);
            const auto raw = sprite
                ? raw_tag(document, sprite->info.definition)
                : std::nullopt;
            if (!sprite || !raw || !raw_equal(
                    *raw,
                    character_frame_wrapper(*required_wrapper, existing->id))) {
                return semantic_error(
                    SettingsGfxErrorCode::resource_conflict, feature, 2);
            }
            wrapper = *required_wrapper;
        } else {
            for (const auto& sprite : model.sprites()) {
                const auto raw = raw_tag(document, sprite.info.definition);
                if (raw && raw_equal(*raw,
                        character_frame_wrapper(
                            sprite.info.character_id, existing->id))) {
                    if (wrapper) {
                        return semantic_error(
                            SettingsGfxErrorCode::resource_conflict,
                            feature,
                            2);
                    }
                    wrapper = sprite.info.character_id;
                }
            }
        }
    } else if (required_wrapper) {
        // A published placement may only be retained when its complete exact
        // resource closure can be proven. Inventing a different external image
        // behind an existing wrapper would silently reinterpret third-party
        // content.
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict, feature, 2);
    }

    std::optional<GfxTagInfo> grid{};
    if (existing && wrapper) {
        for (const auto& tag : document.tags()) {
            if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
            const auto raw = raw_tag(document, tag.handle);
            if (!raw || raw->body.size() < 2 ||
                read_u16(raw->body, 0) != *wrapper) {
                continue;
            }
            if (grid || !raw_equal(*raw, character_frame_grid(*wrapper))) {
                return semantic_error(
                    SettingsGfxErrorCode::resource_conflict, feature, 3);
            }
            grid = tag;
        }
        output = {existing->id, *wrapper};
        const auto image_definition =
            top_level_character_definition(document, existing->id);
        const auto wrapper_definition =
            top_level_character_definition(document, *wrapper);
        const auto image_tag = image_definition
            ? top_level_tag(document, image_definition->tag)
            : std::nullopt;
        const auto wrapper_tag = wrapper_definition
            ? top_level_tag(document, wrapper_definition->tag)
            : std::nullopt;
        if (!image_tag || !wrapper_tag) {
            return semantic_error(
                SettingsGfxErrorCode::resource_conflict, feature, 6);
        }
        if (grid && image_tag->ordinal < wrapper_tag->ordinal &&
            wrapper_tag->ordinal < grid->ordinal &&
            grid->ordinal < user_tag->ordinal) {
            return {};
        }

        std::vector<GfxTagHandle> erase{
            image_tag->handle,
            wrapper_tag->handle,
        };
        if (grid) erase.push_back(grid->handle);
        std::array insert{
            character_frame_external(existing->id),
            character_frame_wrapper(*wrapper, existing->id),
            character_frame_grid(*wrapper),
        };
        if (auto error = replace_top_level_before(
                document, first_user, erase, insert, feature);
            !error.success()) {
            return error;
        }
        changed = true;
        return {};
    }

    const std::size_t required_ids = existing ? 1U : 2U;
    auto ids = document.first_free_character_ids(required_ids);
    if (!ids.success() || ids.character_ids.size() != required_ids) {
        return semantic_error(
            SettingsGfxErrorCode::character_id_exhausted, feature);
    }
    output.image = existing ? existing->id : ids.character_ids[0];
    output.wrapper = existing ? ids.character_ids[0] : ids.character_ids[1];

    std::vector<GfxTagHandle> erase{};
    if (existing) {
        const auto definition = top_level_character_definition(
            document, existing->id);
        const auto tag = definition
            ? top_level_tag(document, definition->tag)
            : std::nullopt;
        if (!tag) {
            return semantic_error(
                SettingsGfxErrorCode::resource_conflict, feature, 5);
        }
        erase.push_back(tag->handle);
    }
    std::array tags{
        character_frame_external(output.image),
        character_frame_wrapper(output.wrapper, output.image),
        character_frame_grid(output.wrapper),
    };
    if (auto error = replace_top_level_before(
            document, first_user, erase, tags, feature);
        !error.success()) {
        return error;
    }
    changed = true;
    return {};
}

[[nodiscard]] SettingsGfxError patch_placeholder_red(GfxDocument& document, std::uint16_t id, bool& changed) {
    GfxTagHandle handle{};
    const auto color = placeholder_color(document, id, &handle);
    if (color == PlaceholderColor::red) return {};
    if (color != PlaceholderColor::gray) return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::text_input, 10);
    auto raw = raw_tag(document, handle);
    if (!raw) return semantic_error(SettingsGfxErrorCode::invalid_document, SettingsGfxFeature::text_input);
    constexpr std::array<std::uint8_t, 4> gray{0x50,0x50,0x50,0xFF};
    constexpr std::array<std::uint8_t, 4> red{0xFF,0x00,0x00,0xFF};
    constexpr std::array<std::uint8_t, 7> gray_html{'#','5','0','5','0','5','0'};
    constexpr std::array<std::uint8_t, 7> red_html{'#','F','F','0','0','0','0'};
    const auto g = find_sequence(raw->body, gray);
    const auto gh = find_sequence(raw->body, gray_html);
    if (g.count != 1 || gh.count != 1)
        return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::text_input, 11);
    std::copy(red.begin(), red.end(), raw->body.begin() + static_cast<std::ptrdiff_t>(g.offset));
    std::copy(red_html.begin(), red_html.end(), raw->body.begin() + static_cast<std::ptrdiff_t>(gh.offset));
    if (auto error = apply_one(document, GfxEdit::replace_body(handle, std::move(raw->body)), SettingsGfxFeature::text_input); !error.success()) return error;
    changed = true;
    return {};
}

[[nodiscard]] SettingsGfxError ensure_text_input(GfxDocument& document, bool& changed) {
    changed = false;
    Model model(document);
    TextInputContext text{};
    if (auto error = locate_text_input(model, text); !error.success()) return error;
    if (character_frame_matches(document, text)) return {};
    const auto character_frame_count = named_count(*text.sprite, kCharacterFrame);
    if (character_frame_count > 1U) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::text_input,
            12);
    }
    const auto* existing_frame = character_frame_count == 1U
        ? model.unique(*text.sprite, kCharacterFrame)
        : nullptr;
    const bool had_existing_frame = existing_frame != nullptr;
    std::optional<std::uint16_t> required_wrapper{};
    if (existing_frame) {
        const auto placement = raw_tag(document, existing_frame->tag);
        const auto depth_two_count = static_cast<std::size_t>(std::count_if(
            text.sprite->placements.begin(), text.sprite->placements.end(),
            [](const GfxPlacementInfo* candidate) {
                return candidate->depth == 2;
            }));
        if (!existing_frame->has_character || depth_two_count != 1U ||
            !placement || !raw_equal(
                *placement,
                character_frame_placement(existing_frame->character_id))) {
            return semantic_error(
                SettingsGfxErrorCode::resource_conflict,
                SettingsGfxFeature::text_input,
                12);
        }
        required_wrapper = existing_frame->character_id;
    } else if (model.any_placement_at_depth(*text.sprite, 2)) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::text_input,
            12);
    }
    const auto color = placeholder_color(
        document, text.empty->character_id);
    if (color != PlaceholderColor::gray && color != PlaceholderColor::red) {
        return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::text_input, 13);
    }
    const auto placeholder_id = text.empty->character_id;

    CharacterFrameResource resource{};
    bool resource_changed{};
    if (auto error = ensure_character_frame_resource(
            document,
            text.sprite->info.definition,
            SettingsGfxFeature::text_input,
            required_wrapper,
            resource,
            resource_changed);
        !error.success()) {
        return error;
    }
    changed |= resource_changed;
    Model updated(document);
    if (auto error = locate_text_input(updated, text); !error.success()) return error;
    if (!had_existing_frame) {
        const auto* show = updated.show_frame(*text.sprite);
        if (!show) return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::text_input, 14);
        if (auto error = apply_one(document,
                GfxEdit::insert_before(show->handle, character_frame_placement(resource.wrapper)),
                SettingsGfxFeature::text_input); !error.success()) return error;
        changed = true;
    }
    if (auto error = patch_placeholder_red(document, placeholder_id, changed); !error.success()) return error;
    Model verify(document);
    TextInputContext verified{};
    if (auto error = locate_text_input(verify, verified); !error.success() || !character_frame_matches(document, verified))
        return semantic_error(SettingsGfxErrorCode::verification_failed, SettingsGfxFeature::text_input);
    return {};
}

[[nodiscard]] std::optional<GfxRawTag> make_button_label_wrapper(
    const GfxDocument& document, std::uint16_t caption_id, std::uint16_t new_id) {
    const auto definition = top_level_character_definition(document, caption_id);
    if (!definition || definition->tag_code != kTagDefineSprite) return std::nullopt;
    auto raw = raw_tag(document, definition->tag);
    if (!raw || raw->body.size() < 4) return std::nullopt;
    write_u16(raw->body, 0, new_id);
    constexpr std::array<std::uint8_t, 7> source{'T','e','x','t','_','0',0};
    const auto match = find_sequence(raw->body, source);
    if (match.count != 1) return std::nullopt;
    raw->body.erase(
        raw->body.begin() + static_cast<std::ptrdiff_t>(match.offset + 4),
        raw->body.begin() + static_cast<std::ptrdiff_t>(match.offset + 6));
    return raw;
}

struct ButtonTarget {
    std::uint16_t sprite{};
    bool complete{};
    std::string label_name{};
};

[[nodiscard]] SettingsGfxError collect_button_targets(
    const Model& model, const TextInputContext& text,
    std::vector<ButtonTarget>& targets) {
    targets.clear();
    std::unordered_map<std::uint16_t, std::size_t> target_indices{};
    target_indices.reserve(kMaximumWidgetHosts);
    for (const auto& container : model.sprites()) {
        const auto* input = model.unique(container, kTextInput);
        if (!input || !input->has_character || input->character_id != text.sprite->info.character_id) continue;
        const auto* button = model.unique(container, kButton);
        if (!button || !button->has_character)
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::button_label, 1);
        const auto* sprite = model.sprite(button->character_id);
        if (!sprite) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 2);
        if (sprite->info.frame_count == 0 ||
            !model.first_show_frame(*sprite)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::button_label,
                8);
        }
        const auto* value = model.at_depth(*sprite, 10);
        if (!value || !value->has_name ||
            (value->name != "Text_0" && value->name != "Text_1"))
            return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::button_label, 3);
        const auto* at12 = model.at_depth(*sprite, 12);
        const bool depth12_occupied = model.any_placement_at_depth(*sprite, 12);
        const std::string expected = value->name == "Text_0" ? "Text_1" : "Text_0";
        if (depth12_occupied &&
            (!at12 || !at12->has_name || !at12->has_character ||
                at12->name != expected))
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::button_label, 4);
        const bool complete = at12 != nullptr;
        const auto existing = target_indices.find(sprite->info.character_id);
        if (existing == target_indices.end()) {
            if (targets.size() == kMaximumWidgetHosts) {
                return semantic_error(
                    SettingsGfxErrorCode::incompatible_structure,
                    SettingsGfxFeature::button_label,
                    static_cast<std::uint32_t>(kMaximumWidgetHosts + 1U));
            }
            target_indices.emplace(sprite->info.character_id, targets.size());
            targets.push_back({sprite->info.character_id, complete, expected});
        } else if (targets[existing->second].complete != complete ||
                   targets[existing->second].label_name != expected) {
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::button_label, 5);
        }
    }
    if (targets.empty()) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 5);
    return {};
}

[[nodiscard]] SettingsGfxError ensure_button_labels(GfxDocument& document, bool& changed) {
    changed = false;
    Model model(document);
    TextInputContext text{};
    if (auto error = locate_text_input(model, text); !error.success()) return error;
    std::vector<ButtonTarget> targets{};
    if (auto error = collect_button_targets(model, text, targets); !error.success()) return error;
    if (std::all_of(targets.begin(), targets.end(), [](const auto& target) { return target.complete; })) return {};

    auto id = document.first_free_character_id();
    if (!id.success()) return semantic_error(SettingsGfxErrorCode::character_id_exhausted, SettingsGfxFeature::button_label);
    auto wrapper = make_button_label_wrapper(document, text.caption->character_id, id.character_id);
    if (!wrapper) return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::button_label, 6);
    const auto* first_button = model.sprite(targets.front().sprite);
    if (!first_button) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 7);
    auto first_button_tag = top_level_tag(document, first_button->info.definition);
    if (!first_button_tag)
        return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::button_label, 7);
    for (const auto& target : targets) {
        const auto* candidate = model.sprite(target.sprite);
        const auto candidate_tag = candidate
            ? top_level_tag(document, candidate->info.definition)
            : std::nullopt;
        if (!candidate || !candidate_tag)
            return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 7);
        if (candidate_tag->ordinal < first_button_tag->ordinal) {
            first_button = candidate;
            first_button_tag = candidate_tag;
        }
    }
    if (auto error = apply_one(document,
            GfxEdit::insert_before(first_button->info.definition, std::move(*wrapper)),
            SettingsGfxFeature::button_label); !error.success()) return error;
    changed = true;

    Model current(document);
    std::vector<GfxEdit> placements{};
    placements.reserve(targets.size());
    for (const auto& target : targets) {
        if (target.complete) continue;
        const auto* button = current.sprite(target.sprite);
        if (!button) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::button_label, 8);
        const auto* show = current.first_show_frame(*button);
        if (!show || current.any_placement_at_depth(*button, 12))
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::button_label, 9);
        auto placement = native_named_placement(id.character_id, 12,
            text.host == SettingsGfxHost::game_options ? -9520 : -9120, 0,
            target.label_name);
        placements.push_back(
            GfxEdit::insert_before(show->handle, std::move(placement)));
    }
    if (!placements.empty()) {
        const auto error = document.apply(placements);
        if (!error.success())
            return document_error(SettingsGfxFeature::button_label, error);
    }
    Model verify(document);
    TextInputContext verified_text{};
    std::vector<ButtonTarget> verified{};
    if (auto error = locate_text_input(verify, verified_text); !error.success()) return error;
    if (auto error = collect_button_targets(verify, verified_text, verified); !error.success()) return error;
    if (!std::all_of(verified.begin(), verified.end(), [](const auto& target) { return target.complete; }))
        return semantic_error(SettingsGfxErrorCode::verification_failed, SettingsGfxFeature::button_label);
    return {};
}

[[nodiscard]] GfxRawTag color_frame_external(std::uint16_t id) {
    GfxRawTag tag{.code = kTagDefineExternalImage2};
    append_u16(tag.body, id); append_u16(tag.body, 0);
    tag.body.push_back(13); tag.body.push_back(0);
    append_u16(tag.body, 74); append_u16(tag.body, 66);
    tag.body.push_back(static_cast<std::uint8_t>(kColorFrameExport.size()));
    tag.body.insert(tag.body.end(), kColorFrameExport.begin(), kColorFrameExport.end());
    tag.body.push_back(static_cast<std::uint8_t>(kColorFrameFile.size()));
    tag.body.insert(tag.body.end(), kColorFrameFile.begin(), kColorFrameFile.end());
    return tag;
}

[[nodiscard]] GfxRawTag color_fill_shape(std::uint16_t id) {
    constexpr std::array<std::uint8_t, 29> tail{
        0x5D,0xA8,0x4B,0x1B,0x50,0x96,0x00,0x01,0x00,0x00,0x99,0xCC,0x00,0x10,0x15,
        0x76,0xA3,0x6A,0x7A,0x12,0xC3,0x95,0x2C,0x74,0x5A,0x87,0x2D,0xA8,0x00};
    GfxRawTag tag{.code = 2, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, id);
    tag.body.insert(tag.body.end(), tail.begin(), tail.end());
    return tag;
}

[[nodiscard]] GfxRawTag single_child_sprite(std::uint16_t id, std::uint16_t child) {
    std::vector<std::uint8_t> placement{0x06U};
    append_u16(placement, 1); append_u16(placement, child); placement.push_back(0);
    GfxRawTag tag{.code = kTagDefineSprite, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, id); append_u16(tag.body, 1);
    for (const auto& encoded : {encode_tag(kTagPlaceObject2, placement),
            encode_tag(kTagShowFrame, {}), encode_tag(kTagEnd, {})})
        tag.body.insert(tag.body.end(), encoded.begin(), encoded.end());
    return tag;
}

[[nodiscard]] GfxRawTag color_frame_wrapper(std::uint16_t wrapper, std::uint16_t image) {
    std::vector<std::uint8_t> placement{0x06U,0x10U};
    append_u16(placement, 1); append_u16(placement, image);
    append_matrix(placement, -363, -325, 32768, 32768);
    GfxRawTag tag{.code = kTagDefineSprite, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, wrapper); append_u16(tag.body, 1);
    for (const auto& encoded : {encode_tag(kTagPlaceObject3, placement, true),
            encode_tag(kTagShowFrame, {}), encode_tag(kTagEnd, {})})
        tag.body.insert(tag.body.end(), encoded.begin(), encoded.end());
    return tag;
}

[[nodiscard]] GfxRawTag color_frame_grid(std::uint16_t wrapper) {
    GfxRawTag tag{.code = kTagDefineScalingGrid};
    append_u16(tag.body, wrapper);
    BitWriter rectangle(tag.body);
    rectangle.write(8, 5);
    for (const auto value : {-100,100,-100,100}) rectangle.write_signed(value, 8);
    rectangle.align();
    return tag;
}

void append_color_frame_transform(std::vector<std::uint8_t>& body) {
    BitWriter writer(body);
    writer.write(1,1); writer.write(1,1); writer.write(10,4);
    for (const auto value : {230,230,230,256,26,26,26,0}) writer.write_signed(value,10);
    writer.align();
}

[[nodiscard]] GfxRawTag color_bracket(std::uint16_t id, std::uint16_t depth) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x0EU); append_u16(tag.body, depth); append_u16(tag.body, id);
    append_matrix(tag.body, -487, -320, 65536, 31457);
    append_character_frame_color_transform(tag.body);
    return tag;
}

[[nodiscard]] GfxRawTag color_fill(std::uint16_t id, std::uint16_t depth) {
    GfxRawTag tag{.code = kTagPlaceObject2, .header_form = GfxTagHeaderForm::long_form};
    tag.body.push_back(0x26U); append_u16(tag.body, depth); append_u16(tag.body, id);
    append_matrix(tag.body, -412, -325, 414260, 49807);
    tag.body.insert(tag.body.end(), kColor.begin(), kColor.end()); tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag color_foreground(std::uint16_t id, std::uint16_t depth) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x0EU); append_u16(tag.body, depth); append_u16(tag.body, id);
    append_matrix(tag.body, -487, -330, 692638, 65536);
    append_color_frame_transform(tag.body);
    return tag;
}

[[nodiscard]] GfxRawTag hidden_color_picker(std::uint16_t id) {
    GfxRawTag tag{.code = kTagPlaceObject3};
    tag.body = {0x26U, 0x20U};
    append_u16(tag.body, 2); append_u16(tag.body, id); tag.body.push_back(0);
    tag.body.insert(tag.body.end(), kColorPicker.begin(), kColorPicker.end());
    tag.body.push_back(0); tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag legacy_preview_placement(
    std::uint16_t id, std::uint16_t depth) {
    GfxRawTag tag{.code = kTagPlaceObject3};
    tag.body = {0x26U, 0x20U};
    append_u16(tag.body, depth); append_u16(tag.body, id); tag.body.push_back(0);
    tag.body.insert(tag.body.end(), kLegacyPreview.begin(), kLegacyPreview.end());
    tag.body.push_back(0); tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag legacy_preview_sprite(
    std::uint16_t id, std::uint16_t character_frame,
    std::uint16_t fill, std::uint16_t foreground) {
    GfxRawTag tag{.code = kTagDefineSprite, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, id); append_u16(tag.body, 1);
    for (const auto& child : {color_bracket(character_frame, 1),
            color_fill(fill, 3), color_foreground(foreground, 5)}) {
        const auto encoded = encode_tag(child.code, child.body,
            child.header_form == GfxTagHeaderForm::long_form);
        tag.body.insert(tag.body.end(), encoded.begin(), encoded.end());
    }
    for (const auto& encoded : {encode_tag(kTagShowFrame, {}), encode_tag(kTagEnd, {})})
        tag.body.insert(tag.body.end(), encoded.begin(), encoded.end());
    return tag;
}

struct LegacyColorResources {
    std::uint16_t preview{};
    std::uint16_t fill{};
    std::uint16_t foreground{};
    std::uint16_t depth{};
};

[[nodiscard]] bool legacy_color_matches(
    const GfxDocument& document, const WidgetContext& widgets,
    LegacyColorResources& output) {
    Model model(document);
    bool character_conflict{};
    const auto character_image = find_external(document, kCharacterFrameExport,
        kCharacterFrameFile, 428, 108, character_conflict);
    if (character_conflict || !character_image) return false;
    const auto character_external = top_level_character_definition(
        document, character_image->id);
    const auto character_external_raw = character_external
        ? raw_tag(document, character_external->tag)
        : std::nullopt;
    if (!character_external_raw ||
        !raw_equal(*character_external_raw,
            character_frame_external(character_image->id))) {
        return false;
    }
    std::optional<std::uint16_t> character_wrapper{};
    for (const auto& sprite : model.sprites()) {
        const auto raw = raw_tag(document, sprite.info.definition);
        if (raw && raw_equal(*raw, character_frame_wrapper(sprite.info.character_id, character_image->id))) {
            if (character_wrapper) return false;
            character_wrapper = sprite.info.character_id;
        }
    }
    if (!character_wrapper) return false;
    std::optional<GfxTagInfo> character_grid{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
        const auto raw = raw_tag(document, tag.handle);
        if (!raw || raw->body.size() < 2 || read_u16(raw->body, 0) != *character_wrapper) continue;
        if (character_grid ||
            !raw_equal(*raw, character_frame_grid(*character_wrapper))) {
            return false;
        }
        character_grid = tag;
    }
    const auto* character_wrapper_sprite = model.sprite(*character_wrapper);
    const auto character_external_tag = character_external
        ? top_level_tag(document, character_external->tag)
        : std::nullopt;
    const auto character_wrapper_tag = character_wrapper_sprite
        ? top_level_tag(document, character_wrapper_sprite->info.definition)
        : std::nullopt;
    if (!character_grid || !character_external_tag ||
        !character_wrapper_tag) {
        return false;
    }

    for (const auto depth : {std::uint16_t{7}, std::uint16_t{5}}) {
        std::optional<std::uint16_t> preview_id{};
        bool buttons_ok = true;
        for (const auto button_id : widgets.buttons) {
            const auto* button = model.sprite(button_id);
            const auto* frame = button ? model.at_depth(*button, 3) : nullptr;
            const auto* preview = button ? model.unique(*button, kLegacyPreview) : nullptr;
            if (!button || !frame || !frame->has_name || frame->name != kButtonFrame ||
                frame->tag_code != kTagPlaceObject2 || frame->flags != 0x26U ||
                frame->flags2 != 0 || frame->move || !frame->has_character ||
                !frame->has_matrix ||
                !preview || preview->depth != depth || !preview->has_character ||
                preview->tag_code != kTagPlaceObject3 || !preview->has_visible || preview->visible) {
                buttons_ok = false; break;
            }
            const auto raw = raw_tag(document, preview->tag);
            if (!raw || !raw_equal(*raw, legacy_preview_placement(preview->character_id, depth))) {
                buttons_ok = false; break;
            }
            if (!preview_id) preview_id = preview->character_id;
            else if (*preview_id != preview->character_id) { buttons_ok = false; break; }
        }
        if (!buttons_ok || !preview_id) continue;
        const auto* preview = model.sprite(*preview_id);
        const auto preview_tag = preview
            ? top_level_tag(document, preview->info.definition)
            : std::nullopt;
        if (!preview_tag) continue;
        const auto* bracket = preview ? model.at_depth(*preview, 1) : nullptr;
        const auto* fill = preview ? model.unique(*preview, kColor) : nullptr;
        const auto* foreground = preview ? model.at_depth(*preview, 5) : nullptr;
        if (!preview || !bracket || bracket->character_id != *character_wrapper ||
            !fill || fill->depth != 3 || !fill->has_character || !foreground ||
            !foreground->has_character || fill->character_id == foreground->character_id) continue;
        const auto raw = raw_tag(document, preview->info.definition);
        if (!raw || !raw_equal(*raw, legacy_preview_sprite(*preview_id,
                *character_wrapper, fill->character_id, foreground->character_id))) continue;
        bool color_conflict{};
        const auto color_image = find_external(document, kColorFrameExport,
            kColorFrameFile, 74, 66, color_conflict);
        if (color_conflict || !color_image) continue;
        const auto* fill_sprite = model.sprite(fill->character_id);
        const auto* frame_sprite = model.sprite(foreground->character_id);
        const auto* shape = fill_sprite ? model.at_depth(*fill_sprite, 1) : nullptr;
        if (!fill_sprite || !frame_sprite || !shape || !shape->has_character) continue;
        const auto shape_info = top_level_character_definition(
            document, shape->character_id);
        const auto fill_raw = raw_tag(document, fill_sprite->info.definition);
        const auto frame_raw = raw_tag(document, frame_sprite->info.definition);
        const auto shape_raw = shape_info ? raw_tag(document, shape_info->tag) : std::nullopt;
        if (!shape_info || shape_info->tag_code != 2 || !fill_raw || !frame_raw || !shape_raw ||
            !raw_equal(*fill_raw, single_child_sprite(fill->character_id, shape->character_id)) ||
            !raw_equal(*shape_raw, color_fill_shape(shape->character_id)) ||
            !raw_equal(*frame_raw, color_frame_wrapper(foreground->character_id, color_image->id))) continue;
        const auto color_external = top_level_character_definition(
            document, color_image->id);
        const auto color_external_raw = color_external
            ? raw_tag(document, color_external->tag)
            : std::nullopt;
        if (!color_external_raw ||
            !raw_equal(*color_external_raw,
                color_frame_external(color_image->id))) {
            continue;
        }
        std::optional<GfxTagInfo> color_grid{};
        for (const auto& tag : document.tags()) {
            if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
            const auto raw_grid = raw_tag(document, tag.handle);
            if (!raw_grid || raw_grid->body.size() < 2 ||
                read_u16(raw_grid->body, 0) != foreground->character_id) continue;
            if (color_grid ||
                !raw_equal(*raw_grid,
                    color_frame_grid(foreground->character_id))) {
                color_grid.reset();
                break;
            }
            color_grid = tag;
        }
        const auto color_external_tag = color_external
            ? top_level_tag(document, color_external->tag)
            : std::nullopt;
        const auto shape_tag = top_level_tag(document, shape_info->tag);
        const auto fill_tag = top_level_tag(
            document, fill_sprite->info.definition);
        const auto frame_tag = top_level_tag(
            document, frame_sprite->info.definition);
        if (!color_grid || !color_external_tag || !shape_tag || !fill_tag ||
            !frame_tag) {
            continue;
        }
        const bool dependency_order =
            character_external_tag->ordinal < character_wrapper_tag->ordinal &&
            character_wrapper_tag->ordinal < character_grid->ordinal &&
            character_grid->ordinal < preview_tag->ordinal &&
            color_external_tag->ordinal < shape_tag->ordinal &&
            shape_tag->ordinal < fill_tag->ordinal &&
            fill_tag->ordinal < preview_tag->ordinal &&
            color_external_tag->ordinal < frame_tag->ordinal &&
            frame_tag->ordinal < color_grid->ordinal &&
            color_grid->ordinal < preview_tag->ordinal;
        if (!dependency_order) continue;
        output = {*preview_id, fill->character_id, foreground->character_id, depth};
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> remove_placement_name(
    const GfxRawTag& raw, std::string_view expected) {
    // Migration is deliberately limited to the authored ButtonFrame form:
    // HasCharacter | HasMatrix | HasName in a PlaceObject2 record. Richer
    // PlaceObject3 records can carry opaque extension data whose offsets are
    // not safe to rewrite by deleting the common-prefix name.
    if (raw.code != kTagPlaceObject2 || raw.body.empty() ||
        raw.body.front() != 0x26U) {
        return std::nullopt;
    }
    auto body = raw.body;
    std::size_t offset = 1U;
    if (body.size() < offset + 2U) return std::nullopt;
    const auto flags = body[0];
    offset += 2U;
    if (flags & 0x02U) offset += 2U;
    if (offset > body.size()) return std::nullopt;
    if ((flags & 0x04U) && !skip_matrix(body, offset)) return std::nullopt;
    if ((flags & 0x08U) && !skip_color(body, offset)) return std::nullopt;
    if (flags & 0x10U) offset += 2U;
    if (offset >= body.size() || !(flags & 0x20U)) return std::nullopt;
    const auto begin = offset;
    const auto terminator = std::find(body.begin() + static_cast<std::ptrdiff_t>(offset), body.end(), 0);
    if (terminator == body.end() || std::next(terminator) != body.end())
        return std::nullopt;
    const std::string_view name(reinterpret_cast<const char*>(body.data() + begin),
        static_cast<std::size_t>(std::distance(body.begin() + static_cast<std::ptrdiff_t>(begin), terminator)));
    if (name != expected) return std::nullopt;
    body[0] &= static_cast<std::uint8_t>(~0x20U);
    body.erase(body.begin() + static_cast<std::ptrdiff_t>(begin), std::next(terminator));
    return body;
}

[[nodiscard]] SettingsGfxError migrate_legacy_color(
    GfxDocument& document, const WidgetContext& widgets,
    const LegacyColorResources& legacy) {
    Model model(document);
    std::vector<GfxEdit> edits{};
    edits.reserve(widgets.buttons.size() * 2U);
    for (const auto button_id : widgets.buttons) {
        const auto* button = model.sprite(button_id);
        const auto* preview = button ? model.unique(*button, kLegacyPreview) : nullptr;
        const auto* frame = button ? model.at_depth(*button, 3) : nullptr;
        if (!button || !preview || !frame || !preview->has_character ||
            preview->character_id != legacy.preview ||
            preview->depth != legacy.depth)
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::color_picker, 20);
        const auto frame_raw = raw_tag(document, frame->tag);
        const auto unnamed = frame_raw ? remove_placement_name(*frame_raw, kButtonFrame) : std::nullopt;
        if (!unnamed) return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::color_picker, 21);
        edits.push_back(GfxEdit::erase(preview->tag));
        edits.push_back(GfxEdit::replace_body(frame->tag, *unnamed));
    }
    const auto error = document.apply(edits);
    if (!error.success())
        return document_error(SettingsGfxFeature::color_picker, error);
    return {};
}

[[nodiscard]] SettingsGfxError ensure_legacy_color_resources_before(
    GfxDocument& document,
    GfxTagHandle first_user,
    const LegacyColorResources& legacy,
    bool& changed) {
    changed = false;
    const auto user_tag = top_level_tag(document, first_user);
    if (!user_tag) {
        return semantic_error(
            SettingsGfxErrorCode::structure_not_found,
            SettingsGfxFeature::color_picker,
            22);
    }

    Model model(document);
    bool image_conflict{};
    const auto image = find_external(
        document,
        kColorFrameExport,
        kColorFrameFile,
        74,
        66,
        image_conflict);
    const auto* fill = model.sprite(legacy.fill);
    const auto* foreground = model.sprite(legacy.foreground);
    const auto* shape_placement = fill ? model.at_depth(*fill, 1) : nullptr;
    if (image_conflict || !image || !fill || !foreground ||
        !shape_placement || !shape_placement->has_character) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::color_picker,
            23);
    }

    const auto image_definition = top_level_character_definition(
        document, image->id);
    const auto shape_definition = top_level_character_definition(
        document, shape_placement->character_id);
    const auto fill_definition = top_level_character_definition(
        document, legacy.fill);
    const auto foreground_definition = top_level_character_definition(
        document, legacy.foreground);
    const auto image_tag = image_definition
        ? top_level_tag(document, image_definition->tag)
        : std::nullopt;
    const auto shape_tag = shape_definition
        ? top_level_tag(document, shape_definition->tag)
        : std::nullopt;
    const auto fill_tag = fill_definition
        ? top_level_tag(document, fill_definition->tag)
        : std::nullopt;
    const auto foreground_tag = foreground_definition
        ? top_level_tag(document, foreground_definition->tag)
        : std::nullopt;
    const auto image_raw = image_definition
        ? raw_tag(document, image_definition->tag)
        : std::nullopt;
    const auto shape_raw = shape_definition
        ? raw_tag(document, shape_definition->tag)
        : std::nullopt;
    const auto fill_raw = fill_definition
        ? raw_tag(document, fill_definition->tag)
        : std::nullopt;
    const auto foreground_raw = foreground_definition
        ? raw_tag(document, foreground_definition->tag)
        : std::nullopt;
    if (!image_tag || !shape_tag || !fill_tag || !foreground_tag ||
        !image_raw || !shape_raw || !fill_raw || !foreground_raw ||
        !raw_equal(*image_raw, color_frame_external(image->id)) ||
        !raw_equal(*shape_raw, color_fill_shape(shape_placement->character_id)) ||
        !raw_equal(*fill_raw, single_child_sprite(
            legacy.fill, shape_placement->character_id)) ||
        !raw_equal(*foreground_raw, color_frame_wrapper(
            legacy.foreground, image->id))) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::color_picker,
            24);
    }

    std::optional<GfxTagInfo> grid{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
        const auto raw = raw_tag(document, tag.handle);
        if (!raw || raw->body.size() < 2U ||
            read_u16(raw->body, 0) != legacy.foreground) {
            continue;
        }
        if (grid || !raw_equal(
                *raw, color_frame_grid(legacy.foreground))) {
            return semantic_error(
                SettingsGfxErrorCode::resource_conflict,
                SettingsGfxFeature::color_picker,
                25);
        }
        grid = tag;
    }
    if (!grid) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::color_picker,
            25);
    }

    if (image_tag->ordinal < shape_tag->ordinal &&
        shape_tag->ordinal < fill_tag->ordinal &&
        fill_tag->ordinal < foreground_tag->ordinal &&
        foreground_tag->ordinal < grid->ordinal &&
        grid->ordinal < user_tag->ordinal) {
        return {};
    }

    std::array<GfxTagHandle, 5> erase{
        image_tag->handle,
        shape_tag->handle,
        fill_tag->handle,
        foreground_tag->handle,
        grid->handle,
    };
    std::array insert{
        color_frame_external(image->id),
        color_fill_shape(shape_placement->character_id),
        single_child_sprite(legacy.fill, shape_placement->character_id),
        color_frame_wrapper(legacy.foreground, image->id),
        color_frame_grid(legacy.foreground),
    };
    if (auto error = replace_top_level_before(
            document,
            first_user,
            erase,
            insert,
            SettingsGfxFeature::color_picker);
        !error.success()) {
        return error;
    }
    changed = true;
    return {};
}

[[nodiscard]] GfxRawTag transparent_placement(
    std::uint16_t id, std::uint16_t depth, std::string_view name) {
    GfxRawTag tag{.code = kTagPlaceObject2, .header_form = GfxTagHeaderForm::long_form};
    tag.body.push_back(0x2EU); append_u16(tag.body, depth); append_u16(tag.body, id);
    tag.body.push_back(0);
    BitWriter transform(tag.body);
    transform.write(0,1); transform.write(1,1); transform.write(10,4);
    for (const auto value : {256,256,256,0}) transform.write_signed(value,10);
    transform.align();
    tag.body.insert(tag.body.end(), name.begin(), name.end()); tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag color_picker_sprite(
    const Model& model, std::uint16_t id,
    std::uint16_t character_frame_id, std::uint16_t fill_id,
    std::uint16_t color_frame_id, const SpriteView& button,
    const TextInputContext& text, bool& valid) {
    valid = false;
    const auto* background = model.at_depth(button, 1);
    const auto* value = model.at_depth(button, 10);
    const auto* label = model.at_depth(button, 12);
    if (!background || !value || !label || !background->has_character ||
        !value->has_character || !label->has_character ||
        !value->has_name || !label->has_name || !text.cursor || !text.cursor->has_character ||
        (value->name != "Text_0" && value->name != "Text_1") ||
        (label->name != "Text_0" && label->name != "Text_1") || value->name == label->name)
        return {};
    GfxRawTag tag{.code = kTagDefineSprite, .header_form = GfxTagHeaderForm::long_form};
    append_u16(tag.body, id); append_u16(tag.body, 1);
    auto append_child = [&](const GfxRawTag& child) {
        const auto bytes = encode_tag(child.code, child.body,
            child.header_form == GfxTagHeaderForm::long_form);
        tag.body.insert(tag.body.end(), bytes.begin(), bytes.end());
    };
    const auto background_raw = raw_tag(model.document(), background->tag);
    if (!background_raw) return {};
    append_child(*background_raw);
    append_child(native_named_placement(text.cursor->character_id, 2, 0, 0, kCursor));
    append_child(color_bracket(character_frame_id, 3));
    append_child(color_fill(fill_id, 5));
    append_child(color_foreground(color_frame_id, 7));
    append_child(transparent_placement(value->character_id, 10, value->name));
    const auto label_raw = raw_tag(model.document(), label->tag);
    if (!label_raw) return {};
    append_child(*label_raw);
    const auto show = encode_tag(kTagShowFrame, {});
    const auto end = encode_tag(kTagEnd, {});
    tag.body.insert(tag.body.end(), show.begin(), show.end());
    tag.body.insert(tag.body.end(), end.begin(), end.end());
    valid = true;
    return tag;
}

[[nodiscard]] bool checked_add_size(
    std::size_t& value,
    std::size_t increment) noexcept {
    if (increment > std::numeric_limits<std::size_t>::max() - value)
        return false;
    value += increment;
    return true;
}

[[nodiscard]] bool add_encoded_tag_size(
    std::size_t body_size,
    GfxTagHeaderForm header_form,
    std::size_t& total) noexcept {
    const bool long_form =
        header_form == GfxTagHeaderForm::long_form || body_size >= 63U;
    return checked_add_size(total, long_form ? 6U : 2U) &&
        checked_add_size(total, body_size);
}

[[nodiscard]] bool add_encoded_tag_size(
    const GfxRawTag& tag,
    std::size_t& total) noexcept {
    return add_encoded_tag_size(tag.body.size(), tag.header_form, total);
}

[[nodiscard]] bool color_picker_body_size(
    const Model& model,
    const SpriteView& button,
    const TextInputContext& text,
    std::size_t& result,
    bool& structurally_valid) {
    result = 0;
    structurally_valid = false;
    const auto* background = model.at_depth(button, 1);
    const auto* value = model.at_depth(button, 10);
    const auto* label = model.at_depth(button, 12);
    if (!background || !value || !label || !background->has_character ||
        !value->has_character || !label->has_character ||
        !value->has_name || !label->has_name || !text.cursor ||
        !text.cursor->has_character ||
        (value->name != "Text_0" && value->name != "Text_1") ||
        (label->name != "Text_0" && label->name != "Text_1") ||
        value->name == label->name) {
        return true;
    }
    const auto background_tag = model.document().tag(background->tag);
    const auto label_tag = model.document().tag(label->tag);
    if (!background_tag || !label_tag) return true;
    structurally_valid = true;

    // CharacterId + FrameCount precede the encoded child stream.
    result = 4U;
    if (!add_encoded_tag_size(
            background_tag->body_size,
            background_tag->header_form,
            result)) {
        return false;
    }
    for (const auto& child : {
             native_named_placement(1, 2, 0, 0, kCursor),
             color_bracket(1, 3),
             color_fill(1, 5),
             color_foreground(1, 7),
             transparent_placement(1, 10, value->name)}) {
        if (!add_encoded_tag_size(child, result)) return false;
    }
    if (!add_encoded_tag_size(
            label_tag->body_size,
            label_tag->header_form,
            result) ||
        !checked_add_size(result, 4U)) { // ShowFrame + End
        return false;
    }
    return true;
}

struct ExactColorPickerClosure {
    bool exact{};
    GfxTagHandle anchor{};
    std::uint16_t color_image{};
    std::uint16_t shape{};
    std::uint16_t fill{};
    std::uint16_t foreground{};
    std::uint16_t character_image{};
    std::uint16_t bracket{};
    std::vector<GfxTagHandle> tags{};
    std::vector<GfxRawTag> picker_definitions{};
};

[[nodiscard]] bool color_picker_matches(
    const GfxDocument& document, const TextInputContext& text,
    const WidgetContext& widgets, std::uint16_t& fill_sprite,
    ExactColorPickerClosure* exact_closure = nullptr) {
    fill_sprite = 0;
    if (exact_closure) *exact_closure = {};
    Model model(document);
    std::size_t expected_picker_body_work{};
    const std::size_t picker_body_budget = std::min(
        document.limits().maximum_declared_bytes,
        document.limits().maximum_physical_bytes);
    for (const auto& host : widgets.hosts) {
        const auto* button = model.sprite(host.button);
        std::size_t body_size{};
        bool structurally_valid{};
        if (!button || !color_picker_body_size(
                model,
                *button,
                text,
                body_size,
                structurally_valid) ||
            !structurally_valid ||
            body_size > document.limits().maximum_tag_body_bytes ||
            !checked_add_size(expected_picker_body_work, body_size) ||
            expected_picker_body_work > picker_body_budget) {
            return false;
        }
    }
    std::optional<std::uint16_t> shared_fill{};
    std::optional<std::uint16_t> shared_frame{};
    std::optional<std::uint16_t> shared_bracket{};
    std::optional<GfxTagInfo> earliest_picker{};
    std::unordered_set<std::uint16_t> picker_ids{};
    picker_ids.reserve(widgets.hosts.size());
    bool pickers_precede_users = true;
    std::vector<GfxTagHandle> picker_tags{};
    std::vector<GfxRawTag> picker_definitions{};
    picker_tags.reserve(widgets.hosts.size());
    picker_definitions.reserve(widgets.hosts.size());
    for (const auto& host : widgets.hosts) {
        const auto* container = model.sprite(host.container);
        const auto* button = model.sprite(host.button);
        const auto* button_frame = button
            ? model.at_depth(*button, 3)
            : nullptr;
        if (!container || !button || !button_frame || button_frame->has_name ||
            named_count(*button, kLegacyPreview) != 0 ||
            named_count(*container, kColorPicker) != 1) {
            return false;
        }
        const auto* placement = model.unique(*container, kColorPicker);
        if (!placement || !placement->has_character || placement->depth != 2 ||
            placement->tag_code != kTagPlaceObject3 || !placement->has_visible || placement->visible)
            return false;
        if (!picker_ids.emplace(placement->character_id).second) {
            return false;
        }
        const auto placement_raw = raw_tag(document, placement->tag);
        if (!placement_raw || !raw_equal(*placement_raw, hidden_color_picker(placement->character_id)))
            return false;
        const auto* picker = model.sprite(placement->character_id);
        if (!picker || picker->info.frame_count != 1 || !model.show_frame(*picker)) return false;
        const auto picker_tag = top_level_tag(document, picker->info.definition);
        const auto container_tag = top_level_tag(document, container->info.definition);
        if (!picker_tag || !container_tag) return false;
        pickers_precede_users &= picker_tag->ordinal < container_tag->ordinal;
        if (!earliest_picker || picker_tag->ordinal < earliest_picker->ordinal) {
            earliest_picker = picker_tag;
        }
        const auto* cursor = model.unique(*picker, kCursor);
        const auto* color = model.unique(*picker, kColor);
        const auto* bracket = model.at_depth(*picker, 3);
        const auto* foreground = model.at_depth(*picker, 7);
        const auto* value = model.at_depth(*picker, 10);
        const auto* label = model.at_depth(*picker, 12);
        if (!cursor || cursor->depth != 2 || !color || color->depth != 5 ||
            !bracket || !foreground || !value || !label ||
            !color->has_character || !bracket->has_character || !foreground->has_character)
            return false;
        if (!shared_fill) {
            shared_fill = color->character_id;
            shared_frame = foreground->character_id;
            shared_bracket = bracket->character_id;
        } else if (*shared_fill != color->character_id || *shared_frame != foreground->character_id ||
                   *shared_bracket != bracket->character_id) return false;
        bool expected_valid{};
        const auto expected = button ? color_picker_sprite(model, picker->info.character_id,
            bracket->character_id, color->character_id, foreground->character_id,
            *button, text, expected_valid) : GfxRawTag{};
        const auto actual = raw_tag(document, picker->info.definition);
        if (!button || !expected_valid || !actual || !raw_equal(*actual, expected)) return false;
        picker_tags.push_back(picker_tag->handle);
        picker_definitions.push_back(expected);
    }
    if (!shared_fill || !shared_frame || !shared_bracket || !earliest_picker ||
        picker_ids.size() != widgets.hosts.size()) {
        return false;
    }
    bool frame_conflict{};
    const auto color_image = find_external(document, kColorFrameExport, kColorFrameFile, 74, 66, frame_conflict);
    if (frame_conflict || !color_image) return false;
    const auto* fill_sprite_info = model.sprite(*shared_fill);
    const auto* frame_sprite_info = model.sprite(*shared_frame);
    if (!fill_sprite_info || !frame_sprite_info) return false;
    const auto fill_raw = raw_tag(document, fill_sprite_info->info.definition);
    const auto frame_raw = raw_tag(document, frame_sprite_info->info.definition);
    if (!fill_raw || !frame_raw ||
        !raw_equal(*frame_raw, color_frame_wrapper(*shared_frame, color_image->id))) return false;
    const auto* fill_child = model.at_depth(*fill_sprite_info, 1);
    if (!fill_child || !fill_child->has_character) return false;
    if (!raw_equal(
            *fill_raw,
            single_child_sprite(*shared_fill, fill_child->character_id))) {
        return false;
    }
    const auto shape_info = top_level_character_definition(
        document, fill_child->character_id);
    if (!shape_info || shape_info->tag_code != 2) return false;
    const auto shape = raw_tag(document, shape_info->tag);
    if (!shape || !raw_equal(*shape, color_fill_shape(fill_child->character_id))) return false;
    bool character_conflict{};
    const auto character_image = find_external(document, kCharacterFrameExport, kCharacterFrameFile, 428, 108, character_conflict);
    if (character_conflict || !character_image) return false;
    std::optional<std::uint16_t> unique_character_wrapper{};
    for (const auto& candidate : model.sprites()) {
        const auto candidate_raw = raw_tag(
            document, candidate.info.definition);
        if (!candidate_raw || !raw_equal(
                *candidate_raw,
                character_frame_wrapper(
                    candidate.info.character_id,
                    character_image->id))) {
            continue;
        }
        if (unique_character_wrapper) return false;
        unique_character_wrapper = candidate.info.character_id;
    }
    if (!unique_character_wrapper ||
        *unique_character_wrapper != *shared_bracket) {
        return false;
    }
    const auto* bracket_info = model.sprite(*shared_bracket);
    if (!bracket_info) return false;
    const auto bracket_raw = raw_tag(document, bracket_info->info.definition);
    if (!bracket_raw || !raw_equal(*bracket_raw,
            character_frame_wrapper(*shared_bracket, character_image->id))) return false;

    const auto color_external = top_level_character_definition(
        document, color_image->id);
    const auto character_external = top_level_character_definition(
        document, character_image->id);
    const auto color_external_raw = color_external
        ? raw_tag(document, color_external->tag)
        : std::nullopt;
    const auto character_external_raw = character_external
        ? raw_tag(document, character_external->tag)
        : std::nullopt;
    if (!color_external_raw ||
        !raw_equal(*color_external_raw,
            color_frame_external(color_image->id)) ||
        !character_external_raw ||
        !raw_equal(*character_external_raw,
            character_frame_external(character_image->id))) {
        return false;
    }

    std::optional<GfxTagInfo> color_grid{};
    std::optional<GfxTagInfo> character_grid{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite || tag.code != kTagDefineScalingGrid) continue;
        const auto raw = raw_tag(document, tag.handle);
        if (!raw || raw->body.size() < 2) continue;
        const auto id = read_u16(raw->body, 0);
        if (id == *shared_frame) {
            if (color_grid ||
                !raw_equal(*raw, color_frame_grid(*shared_frame))) {
                return false;
            }
            color_grid = tag;
        }
        if (id == *shared_bracket) {
            if (character_grid ||
                !raw_equal(*raw, character_frame_grid(*shared_bracket))) {
                return false;
            }
            character_grid = tag;
        }
    }
    const auto color_external_tag = color_external
        ? top_level_tag(document, color_external->tag)
        : std::nullopt;
    const auto shape_tag = top_level_tag(document, shape_info->tag);
    const auto fill_tag = top_level_tag(
        document, fill_sprite_info->info.definition);
    const auto frame_tag = top_level_tag(
        document, frame_sprite_info->info.definition);
    const auto character_external_tag = character_external
        ? top_level_tag(document, character_external->tag)
        : std::nullopt;
    const auto bracket_tag = top_level_tag(
        document, bracket_info->info.definition);
    if (!color_grid || !character_grid || !color_external_tag || !shape_tag ||
        !fill_tag || !frame_tag || !character_external_tag || !bracket_tag) {
        return false;
    }
    if (exact_closure) {
        const std::array<GfxTagInfo, 8> closure_tags{
            *color_external_tag,
            *shape_tag,
            *fill_tag,
            *frame_tag,
            *color_grid,
            *character_external_tag,
            *bracket_tag,
            *character_grid,
        };
        // The CharacterFrame resources are shared with TextInput. Repairing
        // an otherwise exact ColorPicker closure must therefore place the
        // closure before both the widget carriers and the TextInput sprite;
        // anchoring only at the first widget can move a valid shared resource
        // after its earlier TextInput consumer.
        const auto anchor = earliest_color_picker_user(
            document, model, text, widgets);
        if (!anchor) return false;
        exact_closure->exact = true;
        exact_closure->anchor = *anchor;
        exact_closure->color_image = color_image->id;
        exact_closure->shape = fill_child->character_id;
        exact_closure->fill = *shared_fill;
        exact_closure->foreground = *shared_frame;
        exact_closure->character_image = character_image->id;
        exact_closure->bracket = *shared_bracket;
        exact_closure->tags.reserve(
            closure_tags.size() + picker_tags.size());
        std::transform(
            closure_tags.begin(),
            closure_tags.end(),
            std::back_inserter(exact_closure->tags),
            [](const GfxTagInfo& tag) { return tag.handle; });
        exact_closure->tags.insert(
            exact_closure->tags.end(),
            picker_tags.begin(),
            picker_tags.end());
        exact_closure->picker_definitions = std::move(picker_definitions);
    }

    const bool dependency_order =
        pickers_precede_users &&
        shape_tag->ordinal < fill_tag->ordinal &&
        fill_tag->ordinal < earliest_picker->ordinal &&
        color_external_tag->ordinal < frame_tag->ordinal &&
        frame_tag->ordinal < color_grid->ordinal &&
        color_grid->ordinal < earliest_picker->ordinal &&
        character_external_tag->ordinal < bracket_tag->ordinal &&
        bracket_tag->ordinal < character_grid->ordinal &&
        character_grid->ordinal < earliest_picker->ordinal;
    if (!dependency_order) return false;
    fill_sprite = *shared_fill;
    return true;
}

[[nodiscard]] SettingsGfxError canonicalize_color_picker_closure(
    GfxDocument& document,
    const ExactColorPickerClosure& closure) {
    if (!closure.exact || !closure.anchor) {
        return semantic_error(
            SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::color_picker,
            18);
    }
    std::vector<GfxRawTag> insert{};
    insert.reserve(8U + closure.picker_definitions.size());
    insert.push_back(character_frame_external(closure.character_image));
    insert.push_back(character_frame_wrapper(
        closure.bracket, closure.character_image));
    insert.push_back(character_frame_grid(closure.bracket));
    insert.push_back(color_frame_external(closure.color_image));
    insert.push_back(color_fill_shape(closure.shape));
    insert.push_back(single_child_sprite(closure.fill, closure.shape));
    insert.push_back(color_frame_wrapper(
        closure.foreground, closure.color_image));
    insert.push_back(color_frame_grid(closure.foreground));
    insert.insert(
        insert.end(),
        closure.picker_definitions.begin(),
        closure.picker_definitions.end());
    return replace_top_level_before(
        document,
        closure.anchor,
        closure.tags,
        insert,
        SettingsGfxFeature::color_picker);
}

[[nodiscard]] SettingsGfxError ensure_color_picker(GfxDocument& document, bool& changed) {
    changed = false;
    // A ColorPicker borrows the button label contract. Complete it first so
    // the generated widget behaves identically regardless of request order.
    bool label_changed{};
    if (auto error = ensure_button_labels(document, label_changed); !error.success()) return error;
    changed |= label_changed;

    Model model(document);
    TextInputContext text{};
    if (auto error = locate_text_input(model, text); !error.success()) return error;
    WidgetContext widgets{};
    if (auto error = locate_widgets(model, text, widgets); !error.success()) return error;
    std::uint16_t existing_fill{};
    ExactColorPickerClosure exact_closure{};
    if (color_picker_matches(
            document, text, widgets, existing_fill, &exact_closure)) {
        return {};
    }
    // An exact ERNativeUI closure can be semantically complete yet appear in
    // an unsafe dependency order after a third-party rewrite. Canonicalize
    // only that byte-exact closure; near matches remain conflicts and are
    // never silently replaced.
    if (exact_closure.exact) {
        if (auto error = canonicalize_color_picker_closure(
                document, exact_closure);
            !error.success()) {
            return error;
        }
        changed = true;
        Model repaired(document);
        if (auto error = locate_text_input(repaired, text); !error.success())
            return error;
        if (auto error = locate_widgets(repaired, text, widgets);
            !error.success()) {
            return error;
        }
        if (!color_picker_matches(
                document, text, widgets, existing_fill)) {
            return semantic_error(
                SettingsGfxErrorCode::verification_failed,
                SettingsGfxFeature::color_picker,
                18);
        }
        return {};
    }

    LegacyColorResources legacy{};
    const bool migrating_legacy = legacy_color_matches(document, widgets, legacy);
    if (!migrating_legacy) {
        for (const auto button_id : widgets.buttons) {
            const auto* button = model.sprite(button_id);
            if (!button || named_count(*button, kLegacyPreview) != 0) {
                return semantic_error(
                    SettingsGfxErrorCode::resource_conflict,
                    SettingsGfxFeature::color_picker,
                    20);
            }
        }
    }
    if (migrating_legacy) {
        if (auto error = migrate_legacy_color(document, widgets, legacy); !error.success()) return error;
        changed = true;
        Model migrated(document);
        if (auto error = locate_text_input(migrated, text); !error.success()) return error;
        if (auto error = locate_widgets(migrated, text, widgets); !error.success()) return error;
        const auto first_user = earliest_widget_user(
            document, migrated, widgets);
        if (!first_user) {
            return semantic_error(
                SettingsGfxErrorCode::structure_not_found,
                SettingsGfxFeature::color_picker,
                11);
        }
        bool resources_changed{};
        if (auto error = ensure_legacy_color_resources_before(
                document, *first_user, legacy, resources_changed);
            !error.success()) {
            return error;
        }
        changed |= resources_changed;
    }

    Model current_model(document);
    for (const auto& host : widgets.hosts) {
        const auto* container = current_model.sprite(host.container);
        if (!container || named_count(*container, kColorPicker) != 0 ||
            current_model.any_placement_at_depth(*container, 2))
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::color_picker, 10);
    }
    const auto first_user = earliest_widget_user(
        document, current_model, widgets);
    if (!first_user)
        return semantic_error(SettingsGfxErrorCode::structure_not_found,
            SettingsGfxFeature::color_picker, 11);

    CharacterFrameResource character{};
    bool character_changed{};
    if (auto error = ensure_character_frame_resource(
            document,
            *first_user,
            SettingsGfxFeature::color_picker,
            std::nullopt,
            character,
            character_changed);
        !error.success()) {
        return error;
    }
    changed |= character_changed;

    Model refreshed(document);
    if (auto error = locate_text_input(refreshed, text); !error.success()) return error;
    if (auto error = locate_widgets(refreshed, text, widgets); !error.success()) return error;
    const auto refreshed_first_user = earliest_widget_user(
        document, refreshed, widgets);
    const auto refreshed_first_user_tag = refreshed_first_user
        ? top_level_tag(document, *refreshed_first_user)
        : std::nullopt;
    if (!refreshed_first_user || !refreshed_first_user_tag)
        return semantic_error(SettingsGfxErrorCode::structure_not_found,
            SettingsGfxFeature::color_picker, 12);

    bool image_conflict{};
    const auto existing_image = find_external(document, kColorFrameExport,
        kColorFrameFile, 74, 66, image_conflict);
    if (image_conflict)
        return semantic_error(SettingsGfxErrorCode::resource_conflict,
            SettingsGfxFeature::color_picker, 13);

    const std::size_t resource_ids = migrating_legacy
        ? 0U
        : (existing_image ? 3U : 4U);
    auto ids = document.first_free_character_ids(
        resource_ids + widgets.hosts.size());
    if (!ids.success() ||
        ids.character_ids.size() != resource_ids + widgets.hosts.size()) {
        return semantic_error(SettingsGfxErrorCode::character_id_exhausted, SettingsGfxFeature::color_picker);
    }
    std::uint16_t fill_id = legacy.fill;
    std::uint16_t frame_id = legacy.foreground;
    std::vector<GfxRawTag> definitions{};
    std::vector<GfxTagHandle> erase{};
    if (!migrating_legacy) {
        std::size_t next{};
        std::uint16_t image_id{};
        if (existing_image) {
            image_id = existing_image->id;
            const auto definition = top_level_character_definition(
                document, image_id);
            const auto tag = definition
                ? top_level_tag(document, definition->tag)
                : std::nullopt;
            const auto raw = definition
                ? raw_tag(document, definition->tag)
                : std::nullopt;
            if (!definition || !tag || !raw ||
                !raw_equal(*raw, color_frame_external(image_id))) {
                return semantic_error(SettingsGfxErrorCode::resource_conflict,
                    SettingsGfxFeature::color_picker, 13);
            }
            if (tag->ordinal >= refreshed_first_user_tag->ordinal) {
                erase.push_back(tag->handle);
                definitions.push_back(*raw);
            }
        } else {
            image_id = ids.character_ids[next++];
            definitions.push_back(color_frame_external(image_id));
        }
        const auto shape_id = ids.character_ids[next++];
        fill_id = ids.character_ids[next++];
        frame_id = ids.character_ids[next++];
        definitions.push_back(color_fill_shape(shape_id));
        definitions.push_back(single_child_sprite(fill_id, shape_id));
        definitions.push_back(color_frame_wrapper(frame_id, image_id));
        definitions.push_back(color_frame_grid(frame_id));
    }

    // Preflight the complete generated closure before constructing any
    // per-host picker body. A replacement movie may provide very large donor
    // placements; multiplying those donors by every widget host must remain
    // bounded by both the semantic host cap and this document's byte limits.
    std::size_t generated_body_bytes{};
    std::size_t generated_encoded_bytes{};
    std::size_t erased_encoded_bytes{};
    for (const auto& definition : definitions) {
        if (!checked_add_size(
                generated_body_bytes, definition.body.size()) ||
            !add_encoded_tag_size(
                definition, generated_encoded_bytes)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                19);
        }
    }
    for (const GfxTagHandle handle : erase) {
        const auto tag = document.tag(handle);
        if (!tag || !add_encoded_tag_size(
                tag->body_size,
                tag->header_form,
                erased_encoded_bytes)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                19);
        }
    }

    std::vector<std::size_t> picker_body_sizes{};
    picker_body_sizes.reserve(widgets.hosts.size());
    for (std::size_t index = 0; index < widgets.hosts.size(); ++index) {
        const auto* button = refreshed.sprite(widgets.hosts[index].button);
        if (!button) {
            return semantic_error(
                SettingsGfxErrorCode::structure_not_found,
                SettingsGfxFeature::color_picker,
                14);
        }
        std::size_t body_size{};
        bool structurally_valid{};
        if (!color_picker_body_size(
                refreshed,
                *button,
                text,
                body_size,
                structurally_valid) ||
            body_size > document.limits().maximum_tag_body_bytes) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                19);
        }
        if (!structurally_valid) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                15);
        }
        picker_body_sizes.push_back(body_size);
        if (!checked_add_size(generated_body_bytes, body_size) ||
            !add_encoded_tag_size(
                body_size,
                GfxTagHeaderForm::long_form,
                generated_encoded_bytes)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                19);
        }

        const auto placement = hidden_color_picker(
            ids.character_ids[resource_ids + index]);
        if (!checked_add_size(
                generated_body_bytes, placement.body.size()) ||
            !add_encoded_tag_size(
                placement, generated_encoded_bytes) ||
            // Inserting a child can force its owning DefineSprite from a
            // short to a long header. Four bytes per host is a conservative
            // upper bound without materializing the edited sprite bodies.
            !checked_add_size(generated_encoded_bytes, 4U)) {
            return semantic_error(
                SettingsGfxErrorCode::incompatible_structure,
                SettingsGfxFeature::color_picker,
                19);
        }
    }

    const auto& limits = document.limits();
    const std::size_t cumulative_body_budget = std::min(
        limits.maximum_declared_bytes,
        limits.maximum_physical_bytes);
    const std::size_t declared_headroom =
        document.declared_length() <= limits.maximum_declared_bytes
        ? limits.maximum_declared_bytes - document.declared_length()
        : 0U;
    const std::size_t physical_headroom =
        document.physical_size() <= limits.maximum_physical_bytes
        ? limits.maximum_physical_bytes - document.physical_size()
        : 0U;
    const std::size_t encoded_growth =
        generated_encoded_bytes > erased_encoded_bytes
        ? generated_encoded_bytes - erased_encoded_bytes
        : 0U;
    if (generated_body_bytes > cumulative_body_budget ||
        encoded_growth > declared_headroom ||
        encoded_growth > physical_headroom) {
        return semantic_error(
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::color_picker,
            19);
    }

    definitions.reserve(definitions.size() + widgets.hosts.size());
    for (std::size_t index = 0; index < widgets.hosts.size(); ++index) {
        const auto* button = refreshed.sprite(widgets.hosts[index].button);
        if (!button) return semantic_error(SettingsGfxErrorCode::structure_not_found, SettingsGfxFeature::color_picker, 14);
        bool valid{};
        auto picker = color_picker_sprite(refreshed, ids.character_ids[resource_ids + index],
            character.wrapper, fill_id, frame_id, *button, text, valid);
        if (!valid) return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::color_picker, 15);
        if (picker.body.size() != picker_body_sizes[index]) {
            return semantic_error(
                SettingsGfxErrorCode::verification_failed,
                SettingsGfxFeature::color_picker,
                19);
        }
        definitions.push_back(std::move(picker));
    }
    if (auto error = replace_top_level_before(
            document,
            *refreshed_first_user,
            erase,
            definitions,
            SettingsGfxFeature::color_picker);
        !error.success()) {
        return error;
    }
    changed = true;

    Model current(document);
    std::vector<GfxEdit> placements{};
    placements.reserve(widgets.hosts.size());
    for (std::size_t index = 0; index < widgets.hosts.size(); ++index) {
        const auto* container = current.sprite(widgets.hosts[index].container);
        if (!container || current.any_placement_at_depth(*container, 2) ||
            current.unique(*container, kColorPicker))
            return semantic_error(SettingsGfxErrorCode::resource_conflict, SettingsGfxFeature::color_picker, 16);
        const auto* show = current.show_frame(*container);
        if (!show) return semantic_error(SettingsGfxErrorCode::incompatible_structure, SettingsGfxFeature::color_picker, 17);
        placements.push_back(GfxEdit::insert_before(
            show->handle,
            hidden_color_picker(ids.character_ids[resource_ids + index])));
    }
    const auto placement_error = document.apply(placements);
    if (!placement_error.success()) {
        return document_error(
            SettingsGfxFeature::color_picker, placement_error);
    }
    Model verify(document);
    TextInputContext verify_text{};
    WidgetContext verify_widgets{};
    if (auto error = locate_text_input(verify, verify_text); !error.success()) return error;
    if (auto error = locate_widgets(verify, verify_text, verify_widgets); !error.success()) return error;
    if (!color_picker_matches(document, verify_text, verify_widgets, existing_fill))
        return semantic_error(SettingsGfxErrorCode::verification_failed, SettingsGfxFeature::color_picker);
    return {};
}

[[nodiscard]] SettingsGfxError validate_presentation_requirements(
    const SettingsGfxRequirements& requirements) noexcept {
    if (requirements.text_input != SettingsGfxPresentation::preserve &&
        requirements.text_input !=
            SettingsGfxPresentation::ensure_character_creation) {
        return semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::text_input);
    }
    if (requirements.color_picker != SettingsGfxPresentation::preserve &&
        requirements.color_picker !=
            SettingsGfxPresentation::ensure_character_creation) {
        return semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::color_picker);
    }
    return {};
}

[[nodiscard]] SettingsGfxError apply_widget_requirements(
    GfxDocument& document,
    const SettingsGfxRequirements& requirements,
    bool& changed) {
    if (requirements.ensure_button_label) {
        bool operation_changed{};
        if (auto error = ensure_button_labels(document, operation_changed);
            !error.success()) {
            return error;
        }
        changed |= operation_changed;
    }
    if (requirements.text_input ==
        SettingsGfxPresentation::ensure_character_creation) {
        bool operation_changed{};
        if (auto error = ensure_text_input(document, operation_changed);
            !error.success()) {
            return error;
        }
        changed |= operation_changed;
    }
    if (requirements.color_picker ==
        SettingsGfxPresentation::ensure_character_creation) {
        bool operation_changed{};
        if (auto error = ensure_color_picker(document, operation_changed);
            !error.success()) {
            return error;
        }
        changed |= operation_changed;
    }
    return {};
}

[[nodiscard]] SettingsGfxError verify_widget_requirements(
    const SettingsGfxInspection& inspection,
    const SettingsGfxRequirements& requirements) noexcept {
    if (requirements.text_input ==
            SettingsGfxPresentation::ensure_character_creation &&
        !inspection.character_name_text_input) {
        return semantic_error(
            SettingsGfxErrorCode::verification_failed,
            SettingsGfxFeature::text_input);
    }
    if (requirements.color_picker ==
            SettingsGfxPresentation::ensure_character_creation &&
        !inspection.character_creation_color_picker) {
        return semantic_error(
            SettingsGfxErrorCode::verification_failed,
            SettingsGfxFeature::color_picker);
    }
    return {};
}

void populate_widget_inspection(
    const GfxDocument& document,
    const Model& model,
    const TextInputContext& text,
    SettingsGfxInspection& inspection) {
    inspection.host = text.host;
    inspection.text_input_sprite = text.sprite->info.character_id;
    inspection.character_name_text_input =
        character_frame_matches(document, text);

    // A valid TextInput library is sufficient for widget-only inspection.
    // Button/ComboBox/Slider discovery is optional until a transform actually
    // requests ButtonLabel or ColorPicker support.
    WidgetContext widgets{};
    if (const auto error = locate_widgets(model, text, widgets);
        error.success()) {
        inspection.color_picker_host_count =
            static_cast<std::uint16_t>(widgets.hosts.size());
        inspection.character_creation_color_picker = color_picker_matches(
            document,
            text,
            widgets,
            inspection.color_picker_fill_sprite);
    }
}

void populate_panel_inspection(
    const PanelContract& contract,
    const PanelContext& panel,
    SettingsGfxInspection& inspection) noexcept {
    inspection.*(contract.sprite) = panel.panel->info.character_id;
    inspection.*(contract.item_character) = panel.item_character;
    inspection.*(contract.rows) = panel.row_count;
}

[[nodiscard]] bool has_row_requirements(
    const SettingsGfxRequirements& requirements) noexcept {
    return std::any_of(
        kPanelContracts.begin(),
        kPanelContracts.end(),
        [&](const PanelContract& contract) {
            return (requirements.*(contract.requirement)).has_value();
        });
}

} // namespace

SettingsGfxInspectionResult inspect_settings_gfx(const GfxDocument& document) noexcept {
    SettingsGfxInspectionResult result{};
    if (!document.valid()) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_document, SettingsGfxFeature::document);
        return result;
    }
    try {
        Model model(document);
        TextInputContext text{};
        const auto text_error = locate_text_input(model, text);

        PanelContext game{};
        PanelContext camera{};
        const auto game_error = locate_panel(
            model, kPanelContracts[0], nullptr, game);
        if (game_error.success()) {
            const auto camera_error = locate_panel(
                model, kPanelContracts[1], game.window_list, camera);
            if (!camera_error.success()) { result.error = camera_error; return result; }
            result.inspection.window_list_sprite = game.window_list->info.character_id;
            populate_panel_inspection(
                kPanelContracts[0], game, result.inspection);
            populate_panel_inspection(
                kPanelContracts[1], camera, result.inspection);
            for (std::size_t index = 2;
                 index < kPanelContracts.size();
                 ++index) {
                PanelContext panel{};
                if (const auto error = locate_panel(
                        model,
                        kPanelContracts[index],
                        game.window_list,
                        panel);
                    error.success()) {
                    populate_panel_inspection(
                        kPanelContracts[index], panel, result.inspection);
                }
            }
            result.inspection.host = SettingsGfxHost::game_options;
            TabStripContext tabs{};
            if (const auto tab_error = locate_tab_strip(model, tabs);
                !tab_error.success()) {
                result.error = tab_error;
                return result;
            }
            result.inspection.tab_list_sprite =
                tabs.tab_list.sprite->info.character_id;
            result.inspection.back_tab_list_sprite =
                tabs.back_tab_list.sprite->info.character_id;
            result.inspection.tab_list_cells = tabs.cell_count;
            result.inspection.back_tab_list_cells = tabs.cell_count;
            result.inspection.authored_tab_slot_capacity =
                static_cast<std::uint16_t>(tabs.cell_count - 1U);
            if (text_error.success() && text.host != SettingsGfxHost::game_options) {
                result.error = semantic_error(SettingsGfxErrorCode::incompatible_structure,
                    SettingsGfxFeature::text_input, 30);
                return result;
            }
        } else {
            // Advanced Graphics contains the shared widget library but no
            // ControllSetting/CameraSetting pair. A Game Options-like movie
            // with a damaged panel must not be misclassified as that host.
            if (!text_error.success() || text.host != SettingsGfxHost::advanced_settings) {
                result.error = game_error;
                return result;
            }
            result.inspection.host = SettingsGfxHost::advanced_settings;
        }
        result.inspection.gfx_version = document.version();
        result.inspection.declared_length = document.declared_length();
        if (text_error.success()) {
            populate_widget_inspection(
                document, model, text, result.inspection);
        }
        return result;
    } catch (...) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_document, SettingsGfxFeature::document);
        return result;
    }
}

SettingsGfxInspectionResult inspect_settings_widgets_gfx(
    const GfxDocument& document) noexcept {
    SettingsGfxInspectionResult result{};
    if (!document.valid()) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_document,
            SettingsGfxFeature::document);
        return result;
    }
    try {
        Model model(document);
        TextInputContext text{};
        if (auto error = locate_text_input(model, text); !error.success()) {
            result.error = error;
            return result;
        }
        result.inspection.gfx_version = document.version();
        result.inspection.declared_length = document.declared_length();
        populate_widget_inspection(
            document, model, text, result.inspection);
        return result;
    } catch (...) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_document,
            SettingsGfxFeature::document);
        return result;
    }
}

SettingsGfxTransformResult transform_settings_gfx(
    GfxDocument source, const SettingsGfxRequirements& requirements) noexcept {
    SettingsGfxTransformResult result{};

    // Request validation deliberately precedes document inspection. A caller
    // error has a deterministic result even when the supplied movie is also
    // invalid, and no unsupported enum value can influence host discovery.
    if (auto error = validate_presentation_requirements(requirements);
        !error.success()) {
        result.error = error;
        return result;
    }
    for (const PanelContract& contract : kPanelContracts) {
        const auto& requested = requirements.*(contract.requirement);
        if (requested && *requested > kMaximumRows) {
            result.error = semantic_error(
                SettingsGfxErrorCode::invalid_argument,
                contract.feature,
                *requested);
            return result;
        }
    }
    if (requirements.minimum_tab_slots &&
        (*requirements.minimum_tab_slots < kMinimumRequestedTabSlots ||
         *requirements.minimum_tab_slots > kMaximumRequestedTabSlots)) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::tab_slots,
            *requirements.minimum_tab_slots);
        return result;
    }

    const auto before = inspect_settings_gfx(source);
    if (!before.success()) { result.error = before.error; return result; }
    result.before = before.inspection;
    try {
        bool changed{};
        auto perform = [&](SettingsGfxError error, bool operation_changed) -> bool {
            if (!error.success()) { result.error = error; return false; }
            changed |= operation_changed;
            return true;
        };

        if (has_row_requirements(requirements) &&
            before.inspection.host != SettingsGfxHost::game_options) {
            const auto requested = std::find_if(
                kPanelContracts.begin(),
                kPanelContracts.end(),
                [&](const PanelContract& contract) {
                    return (requirements.*(contract.requirement)).has_value();
                });
            result.error = semantic_error(
                SettingsGfxErrorCode::unsupported_document,
                requested != kPanelContracts.end()
                    ? requested->feature
                    : SettingsGfxFeature::document);
            return result;
        }
        if (requirements.minimum_tab_slots &&
            before.inspection.host != SettingsGfxHost::game_options) {
            result.error = semantic_error(
                SettingsGfxErrorCode::unsupported_document,
                SettingsGfxFeature::tab_slots,
                *requirements.minimum_tab_slots);
            return result;
        }
        for (const PanelContract& contract : kPanelContracts) {
            const auto& requested = requirements.*(contract.requirement);
            if (!requested) continue;
            const bool operation_changed =
                before.inspection.*(contract.rows) < *requested;
            if (!perform(
                    grow_panel(
                        source,
                        contract,
                        before.inspection.window_list_sprite,
                        *requested),
                    operation_changed)) {
                return result;
            }
        }
        if (requirements.minimum_tab_slots) {
            const bool operation_changed =
                before.inspection.authored_tab_slot_capacity <
                    *requirements.minimum_tab_slots;
            if (!perform(
                    grow_tab_strip(
                        source, *requirements.minimum_tab_slots),
                    operation_changed)) {
                return result;
            }
        }
        if (auto error = apply_widget_requirements(
                source, requirements, changed);
            !error.success()) {
            result.error = error;
            return result;
        }

        const auto after = inspect_settings_gfx(source);
        if (!after.success()) {
            result.error = semantic_error(SettingsGfxErrorCode::verification_failed,
                SettingsGfxFeature::verification, after.error.detail);
            return result;
        }
        for (const PanelContract& contract : kPanelContracts) {
            const auto& requested = requirements.*(contract.requirement);
            if (requested &&
                after.inspection.*(contract.rows) < *requested) {
                result.error = semantic_error(
                    SettingsGfxErrorCode::verification_failed,
                    contract.feature);
                return result;
            }
        }
        if (requirements.minimum_tab_slots &&
            after.inspection.authored_tab_slot_capacity <
                *requirements.minimum_tab_slots) {
            result.error = semantic_error(
                SettingsGfxErrorCode::verification_failed,
                SettingsGfxFeature::tab_slots);
            return result;
        }
        if (auto error = verify_widget_requirements(
                after.inspection, requirements);
            !error.success()) {
            result.error = error;
            return result;
        }
        result.after = after.inspection;
        result.document = std::move(source);
        result.status = changed ? SettingsGfxTransformStatus::modified
                                : SettingsGfxTransformStatus::unchanged;
        return result;
    } catch (...) {
        result.error = semantic_error(SettingsGfxErrorCode::mutation_failed,
            SettingsGfxFeature::document);
        return result;
    }
}

SettingsGfxTransformResult transform_settings_widgets_gfx(
    GfxDocument source,
    const SettingsGfxRequirements& requirements) noexcept {
    SettingsGfxTransformResult result{};

    if (auto error = validate_presentation_requirements(requirements);
        !error.success()) {
        result.error = error;
        return result;
    }
    for (const PanelContract& contract : kPanelContracts) {
        const auto& requested = requirements.*(contract.requirement);
        if (requested) {
            result.error = semantic_error(
                SettingsGfxErrorCode::invalid_argument,
                contract.feature,
                *requested);
            return result;
        }
    }
    if (requirements.minimum_tab_slots) {
        result.error = semantic_error(
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::tab_slots,
            *requirements.minimum_tab_slots);
        return result;
    }

    const auto before = inspect_settings_widgets_gfx(source);
    if (!before.success()) {
        result.error = before.error;
        return result;
    }
    result.before = before.inspection;
    try {
        bool changed{};
        if (auto error = apply_widget_requirements(
                source, requirements, changed);
            !error.success()) {
            result.error = error;
            return result;
        }

        const auto after = inspect_settings_widgets_gfx(source);
        if (!after.success()) {
            result.error = semantic_error(
                SettingsGfxErrorCode::verification_failed,
                SettingsGfxFeature::verification,
                after.error.detail);
            return result;
        }
        if (auto error = verify_widget_requirements(
                after.inspection, requirements);
            !error.success()) {
            result.error = error;
            return result;
        }
        result.after = after.inspection;
        result.document = std::move(source);
        result.status = changed ? SettingsGfxTransformStatus::modified
                                : SettingsGfxTransformStatus::unchanged;
        return result;
    } catch (...) {
        result.error = semantic_error(
            SettingsGfxErrorCode::mutation_failed,
            SettingsGfxFeature::document);
        return result;
    }
}

const char* settings_gfx_error_name(SettingsGfxErrorCode code) noexcept {
    switch (code) {
    case SettingsGfxErrorCode::none: return "none";
    case SettingsGfxErrorCode::invalid_argument: return "invalid_argument";
    case SettingsGfxErrorCode::invalid_document: return "invalid_document";
    case SettingsGfxErrorCode::unsupported_document: return "unsupported_document";
    case SettingsGfxErrorCode::structure_not_found: return "structure_not_found";
    case SettingsGfxErrorCode::ambiguous_structure: return "ambiguous_structure";
    case SettingsGfxErrorCode::incompatible_structure: return "incompatible_structure";
    case SettingsGfxErrorCode::resource_conflict: return "resource_conflict";
    case SettingsGfxErrorCode::character_id_exhausted: return "character_id_exhausted";
    case SettingsGfxErrorCode::mutation_failed: return "mutation_failed";
    case SettingsGfxErrorCode::verification_failed: return "verification_failed";
    default: return "unknown";
    }
}

const char* settings_gfx_feature_name(SettingsGfxFeature feature) noexcept {
    switch (feature) {
    case SettingsGfxFeature::none: return "none";
    case SettingsGfxFeature::document: return "document";
    case SettingsGfxFeature::game_options_rows: return "game_options_rows";
    case SettingsGfxFeature::camera_options_rows: return "camera_options_rows";
    case SettingsGfxFeature::display_rows: return "display_rows";
    case SettingsGfxFeature::sound_rows: return "sound_rows";
    case SettingsGfxFeature::network_rows: return "network_rows";
    case SettingsGfxFeature::keyboard_mouse_rows:
        return "keyboard_mouse_rows";
    case SettingsGfxFeature::graphics_rows: return "graphics_rows";
    case SettingsGfxFeature::tab_slots: return "tab_slots";
    case SettingsGfxFeature::button_label: return "button_label";
    case SettingsGfxFeature::text_input: return "text_input";
    case SettingsGfxFeature::color_picker: return "color_picker";
    case SettingsGfxFeature::verification: return "verification";
    default: return "unknown";
    }
}

const char* settings_gfx_host_name(SettingsGfxHost host) noexcept {
    switch (host) {
    case SettingsGfxHost::unknown: return "unknown";
    case SettingsGfxHost::game_options: return "game_options";
    case SettingsGfxHost::advanced_settings: return "advanced_settings";
    default: return "unknown";
    }
}

} // namespace erui::gfx
