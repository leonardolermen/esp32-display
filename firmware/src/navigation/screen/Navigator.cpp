#include "navigation/screen/Navigator.h"

#include "board/ScreenGeometry.h"

namespace {

constexpr uint32_t SLIDE_MILLIS = 250;
constexpr int32_t DOT_SIZE = 6;
constexpr int32_t ACTIVE_DOT_WIDTH = 12;

void moveX(void* object, int32_t x)
{
    lv_obj_set_x(static_cast<lv_obj_t*>(object), x);
}

}

void Navigator::add(std::unique_ptr<Page> page)
{
    slots.push_back(Slot{std::move(page)});
}

void Navigator::begin()
{
    index = std::make_unique<PageIndex>(slots.size());

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, onGesture, LV_EVENT_GESTURE, this);

    for (Slot& slot : slots) {
        slot.container = lv_obj_create(screen);
        lv_obj_remove_style_all(slot.container);
        lv_obj_set_size(slot.container, LV_PCT(100), LV_PCT(100));
        lv_obj_add_flag(slot.container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(slot.container, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_clear_flag(slot.container, LV_OBJ_FLAG_SCROLLABLE);
        slot.page->build(slot.container);
    }

    dots = lv_obj_create(screen);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, DOT_SIZE);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 5, 0);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -7);
    for (Slot& slot : slots) {
        slot.dot = lv_obj_create(dots);
        lv_obj_remove_style_all(slot.dot);
        lv_obj_set_style_radius(slot.dot, 3, 0);
        lv_obj_set_style_bg_color(slot.dot, lv_color_white(), 0);
    }

    lv_obj_clear_flag(slots[0].container, LV_OBJ_FLAG_HIDDEN);
    slots[0].page->onShow();
    refreshDots();
}

void Navigator::tick(uint32_t nowMillis)
{
    slots[index->current()].page->onTick(nowMillis);
}

void Navigator::goHome()
{
    if (index->current() != 0) {
        show(0, LV_DIR_RIGHT);
    }
}

void Navigator::onGesture(lv_event_t* event)
{
    Navigator* self = static_cast<Navigator*>(lv_event_get_user_data(event));
    lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_active());
    size_t before = self->index->current();

    if (direction == LV_DIR_LEFT && before + 1 < self->slots.size()) {
        self->show(before + 1, direction);
    }
    if (direction == LV_DIR_RIGHT && before > 0) {
        self->show(before - 1, direction);
    }
}

void Navigator::show(size_t target, lv_dir_t direction)
{
    Slot& leaving = slots[index->current()];
    Slot& entering = slots[target];

    leaving.page->onHide();
    lv_obj_add_flag(leaving.container, LV_OBJ_FLAG_HIDDEN);

    int32_t from = direction == LV_DIR_LEFT ? SCREEN_WIDTH : -SCREEN_WIDTH;
    lv_obj_set_x(entering.container, from);
    lv_obj_clear_flag(entering.container, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t slide;
    lv_anim_init(&slide);
    lv_anim_set_var(&slide, entering.container);
    lv_anim_set_values(&slide, from, 0);
    lv_anim_set_duration(&slide, SLIDE_MILLIS);
    lv_anim_set_path_cb(&slide, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&slide, moveX);
    lv_anim_start(&slide);

    index->jumpTo(target);
    entering.page->onShow();
    refreshDots();
    lv_obj_move_foreground(dots);
}

void Navigator::refreshDots()
{
    for (size_t position = 0; position < slots.size(); position++) {
        bool active = position == index->current();

        lv_obj_set_size(slots[position].dot, active ? ACTIVE_DOT_WIDTH : DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_bg_opa(slots[position].dot, active ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}
