#include "lvgl.h"
#include "font_awesome_symbols.h"

LV_FONT_DECLARE(FONT_SY_20);
LV_FONT_DECLARE(font_awesome_30_4);

lv_obj_t *chat_message_label_;
lv_obj_t *status_label_;
void ui_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_text_font(screen, &FONT_SY_20, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    lv_obj_t *container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    lv_obj_t *status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, FONT_SY_20.line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    lv_obj_t *content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    lv_obj_t *emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);                   // 限制宽度为屏幕宽度的 90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);           // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
    lv_label_set_text(chat_message_label_, "");

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

    lv_obj_t *network_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(network_label_, &font_awesome_30_4, 0);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI);
    // lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    lv_obj_t *notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    // lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    // lv_obj_set_style_text_font(status_label_, &font_awesome_30_4, 0);
    lv_label_set_text(status_label_, "待命");

    lv_obj_t *mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    // lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    lv_obj_t *battery_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(battery_label_, &font_awesome_30_4, 0);
    lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING);
    // lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
}
