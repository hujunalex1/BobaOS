/* Watch-style home screen implementation. */
#include "home_screen.hpp"
#include "window_mgr.hpp"
#include "theme.hpp"
#include "compact_ui.hpp"
#include "focus_pager.hpp"
#include "app_icons.hpp"
#include "core/power_mgr.hpp"
#include "core/settings_store.hpp"
#include "core/calendar_model.hpp"
#include "core/qmi8658.hpp"
#include "core/screen_orientation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

namespace ui {

static lv_obj_t *s_app_list = nullptr;
namespace {
lv_obj_t *desktop=nullptr,*allApps=nullptr,*homeTime=nullptr,*homeDate=nullptr,*homePill=nullptr;
lv_obj_t *homeHour=nullptr,*homeMin=nullptr,*homePaw=nullptr;
lv_obj_t *desktopTab=nullptr,*appsTab=nullptr;
lv_obj_t *candyDock=nullptr,*candyDot0=nullptr,*candyDot1=nullptr;
bool candyMode=false,candyAppsPage=false,swipeHandled=false;
std::vector<lv_obj_t *> terminalRows;
size_t cursor=0;
FocusPager focusApps;
namespace orderModel=sys::app_order;
orderModel::Order menuOrder,dragBefore;
struct MenuRow {lv_obj_t *object;size_t app;lv_obj_t *number=nullptr;};
std::vector<MenuRow> menuRows;
lv_obj_t *homeParent=nullptr,*menuList=nullptr,*arrangePage=nullptr,*ghost=nullptr,*noticeBox=nullptr,*noticeLabel=nullptr;
lv_obj_t *pressRow=nullptr;
lv_indev_t *dragInput=nullptr;
lv_timer_t *tileTimer=nullptr;
lv_point_t pressPoint{},dragPoint{},grabOffset{};
size_t draggedApp=0;
unsigned menuColumns=2;
int menuRowHeight=53;
bool dragging=false,pressMoved=false,suppressClick=false,arrangePending=false,closeArrangePending=false;
bool waitingSave=false;
uint32_t nextDragTick=0,noticeUntil=0;
uint32_t focusSelectedId=0;
orderModel::SaveState shownSave=orderModel::SaveState::Saved;

// --- 重力感应摇摇乐 (Boba Physics) 模型定义 ---
struct BobaBall {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float r = 11.0f;
    float mass = 1.0f;
    lv_obj_t *obj = nullptr;
};

static constexpr int kNumBalls = 6;
static BobaBall s_balls[kNumBalls];
static lv_timer_t *s_physicsTimer = nullptr;
static uint32_t s_imuLease = 0;
static float s_filteredGx = 0.0f;
static float s_filteredGy = 0.38f;
static uint32_t s_bobaTick = 0;
static float s_lastShakeMag = 1.0f;
static uint32_t s_lastShakeTick = 0;
static float s_lastSampleX = 0.0f;
static float s_lastSampleY = 0.0f;
static float s_lastSampleZ = 1.0f;
static uint32_t s_lastMotionTick = 0;
static bool s_isSnoozing = false;

static void popBalls(float strength = 1.0f) {
    for(int i = 0; i < kNumBalls; ++i) {
        float rvx = float((std::rand() % 120) - 60) / 10.0f * strength;
        float rvy = -float((std::rand() % 60) + 50) / 10.0f * strength;
        s_balls[i].vx += rvx;
        s_balls[i].vy += rvy;
    }
}

static void physicsTick(lv_timer_t *) {
    if(!sys::Power::isScreenOn() || !WindowManager::instance().atHome() || !desktop || lv_obj_is_hidden(desktop)) {
        return;
    }

    s_bobaTick++;

    const uint32_t now = lv_tick_get();
    if(s_lastMotionTick == 0) s_lastMotionTick = now;

    float targetGx = 0.0f;
    float targetGy = 0.38f;

    if(s_imuLease != 0) {
        sys::AccelerationSample sample;
        const bool ok = sys::Qmi8658::temporaryAcceleration(s_imuLease, sample);
        if(ok) {
            sys::ScreenOrientation::mapAcceleration(sample.value);

            // 静止休眠判定 (Stationary Snooze Detection)
            const float dAccel = std::abs(sample.value.x - s_lastSampleX) +
                                 std::abs(sample.value.y - s_lastSampleY) +
                                 std::abs(sample.value.z - s_lastSampleZ);
            s_lastSampleX = sample.value.x;
            s_lastSampleY = sample.value.y;
            s_lastSampleZ = sample.value.z;

            if(dAccel > 0.08f) {
                // 检测到明显移动/拿起手表：刷新活跃时间，若此前在降频休眠则立即拉满 30Hz
                s_lastMotionTick = now;
                if(s_isSnoozing && s_physicsTimer) {
                    s_isSnoozing = false;
                    lv_timer_set_period(s_physicsTimer, 33);
                    popBalls(0.4f); // 唤醒时轻柔微弹跳
                }
            } else {
                // 几乎无加速度变化：手表平放静置中
                const uint32_t stillMs = now - s_lastMotionTick;
                // 静置 8 秒：降低物理刷新率至 6Hz (160ms)，节省 CPU 算力
                if(stillMs >= 8000 && !s_isSnoozing && s_physicsTimer) {
                    s_isSnoozing = true;
                    lv_timer_set_period(s_physicsTimer, 160);
                }
                // 静置 25 秒：自动提前熄屏省电
                if(stillMs >= 25000) {
                    s_lastMotionTick = now;
                    sys::Power::automaticScreenOff();
                }
            }

            const float magSq = sample.value.x * sample.value.x + sample.value.y * sample.value.y + sample.value.z * sample.value.z;
            const float mag = std::sqrt(magSq);

            if(std::abs(mag - s_lastShakeMag) > 1.3f && int32_t(now - s_lastShakeTick) > 350) {
                s_lastShakeTick = now;
                s_lastMotionTick = now;
                popBalls(1.1f);
            }
            s_lastShakeMag = mag;

            const float tiltNorm = sample.value.x * sample.value.x + sample.value.y * sample.value.y;
            if(tiltNorm > 0.04f) {
                targetGx = sample.value.x * 2.2f;
                targetGy = sample.value.y * 2.2f;
            } else {
                targetGx = std::sin(float(s_bobaTick) * 0.06f) * 0.16f;
                targetGy = 0.38f;
            }
        }
    } else {
        targetGx = std::sin(float(s_bobaTick) * 0.06f) * 0.16f;
        targetGy = 0.38f;
    }

    s_filteredGx = s_filteredGx * 0.65f + targetGx * 0.35f;
    s_filteredGy = s_filteredGy * 0.65f + targetGy * 0.35f;

    constexpr float kMinX = 8.0f;
    constexpr float kMaxX = 220.0f;
    constexpr float kMinY = 32.0f;
    constexpr float kMaxY = 106.0f;

    for(int i = 0; i < kNumBalls; ++i) {
        auto &b = s_balls[i];
        if(!b.obj) continue;

        b.vx += s_filteredGx;
        b.vy += s_filteredGy;

        b.vx *= 0.93f;
        b.vy *= 0.93f;

        b.x += b.vx;
        b.y += b.vy;

        const float r = b.r;
        if(b.x < kMinX + r) {
            b.x = kMinX + r;
            b.vx = -b.vx * 0.65f;
        } else if(b.x > kMaxX - r) {
            b.x = kMaxX - r;
            b.vx = -b.vx * 0.65f;
        }

        if(b.y < kMinY + r) {
            b.y = kMinY + r;
            b.vy = -b.vy * 0.65f;
        } else if(b.y > kMaxY - r) {
            b.y = kMaxY - r;
            b.vy = -b.vy * 0.65f;
        }
    }

    // 静止休眠模式下小球已经静止沉底，跳过 O(N^2) 两两碰撞循环
    if(!s_isSnoozing) {
        for(int i = 0; i < kNumBalls; ++i) {
            for(int j = i + 1; j < kNumBalls; ++j) {
                auto &b1 = s_balls[i];
                auto &b2 = s_balls[j];
                if(!b1.obj || !b2.obj) continue;

                const float dx = b2.x - b1.x;
                const float dy = b2.y - b1.y;
                const float distSq = dx * dx + dy * dy;
                const float minDist = b1.r + b2.r;

                if(distSq < minDist * minDist && distSq > 0.0001f) {
                    const float dist = std::sqrt(distSq);
                    const float overlap = 0.5f * (minDist - dist);
                    const float nx = dx / dist;
                    const float ny = dy / dist;

                    b1.x -= nx * overlap;
                    b1.y -= ny * overlap;
                    b2.x += nx * overlap;
                    b2.y += ny * overlap;

                    const float kx = b1.vx - b2.vx;
                    const float ky = b1.vy - b2.vy;
                    const float p = 2.0f * (nx * kx + ny * ky) / (b1.mass + b2.mass);

                    b1.vx -= p * b2.mass * nx * 0.78f;
                    b1.vy -= p * b2.mass * ny * 0.78f;
                    b2.vx += p * b1.mass * nx * 0.78f;
                    b2.vy += p * b1.mass * ny * 0.78f;
                }
            }
        }
    }

    for(int i = 0; i < kNumBalls; ++i) {
        auto &b = s_balls[i];
        if(!b.obj) continue;
        lv_obj_set_pos(b.obj, int(std::round(b.x - b.r)), int(std::round(b.y - b.r)));
    }
}

void updateTiles(lv_timer_t *);
void shift(lv_obj_t *obj,int x,int y);
void stopShift(lv_obj_t *obj);

void candySetPage(bool apps, bool animate=false) {
    if(!candyMode || !desktop || !allApps) return;
    if(candyAppsPage == apps && !lv_obj_is_hidden(apps ? allApps : desktop)) return;
    candyAppsPage = apps;
    stopShift(desktop);
    stopShift(allApps);
    if(apps) {
        lv_obj_set_hidden(desktop, true);
        lv_obj_set_hidden(allApps, false);
        if(animate) shift(allApps, 72, 0);
    } else {
        lv_obj_set_hidden(allApps, true);
        lv_obj_set_hidden(desktop, false);
        updateTiles(nullptr);
        if(animate) shift(desktop, -72, 0);
    }
    if(candyDot0 && candyDot1) {
        if(!apps) {
            lv_obj_set_size(candyDot0, 18, 6);
            lv_obj_set_pos(candyDot0, 105, 6);
            lv_obj_set_style_bg_color(candyDot0, theme::accent(), 0);
            lv_obj_set_size(candyDot1, 6, 6);
            lv_obj_set_pos(candyDot1, 129, 6);
            lv_obj_set_style_bg_color(candyDot1, lv_color_hex(0xDCCFC2), 0);
        } else {
            lv_obj_set_size(candyDot0, 6, 6);
            lv_obj_set_pos(candyDot0, 105, 6);
            lv_obj_set_style_bg_color(candyDot0, lv_color_hex(0xDCCFC2), 0);
            lv_obj_set_size(candyDot1, 18, 6);
            lv_obj_set_pos(candyDot1, 117, 6);
            lv_obj_set_style_bg_color(candyDot1, theme::accent(), 0);
        }
    }
}

bool checkCandySwipe(int dx, int dy, int threshold) {
    if(!candyMode || dragging || swipeHandled) return false;
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);
    if(adx >= threshold && adx > ady * 5 / 4) {
        if(!candyAppsPage && dx < 0) {
            swipeHandled = true;
            suppressClick = true;
            pressMoved = true;
            candySetPage(true, true);
            return true;
        } else if(candyAppsPage && dx > 0) {
            swipeHandled = true;
            suppressClick = true;
            pressMoved = true;
            candySetPage(false, true);
            return true;
        }
    }
    return false;
}

void candySwipeEvent(lv_event_t *event) {
    if(!candyMode) return;
    s_lastMotionTick = lv_tick_get();
    if(s_isSnoozing && s_physicsTimer) {
        s_isSnoozing = false;
        lv_timer_set_period(s_physicsTimer, 33);
    }
    const auto code = lv_event_get_code(event);
    auto input = lv_indev_active();
    if(code == LV_EVENT_PRESSED) {
        suppressClick = false;
        pressMoved = false;
        swipeHandled = false;
        if(input) lv_indev_get_point(input, &pressPoint);
    } else if(code == LV_EVENT_PRESSING && input && !dragging) {
        lv_point_t point;
        lv_indev_get_point(input, &point);
        const int dx = point.x - pressPoint.x;
        const int dy = point.y - pressPoint.y;
        if(std::abs(dx) > 8 || std::abs(dy) > 8) pressMoved = true;
        checkCandySwipe(dx, dy, 30);
    } else if(code == LV_EVENT_RELEASED && input && !dragging && !swipeHandled) {
        lv_point_t point;
        lv_indev_get_point(input, &point);
        checkCandySwipe(point.x - pressPoint.x, point.y - pressPoint.y, 26);
    } else if(code == LV_EVENT_GESTURE && input && !dragging && !swipeHandled) {
        const auto dir = lv_indev_get_gesture_dir(input);
        if(dir == LV_DIR_LEFT && !candyAppsPage) {
            swipeHandled = true;
            suppressClick = true;
            pressMoved = true;
            candySetPage(true, true);
        } else if(dir == LV_DIR_RIGHT && candyAppsPage) {
            swipeHandled = true;
            suppressClick = true;
            pressMoved = true;
            candySetPage(false, true);
        }
    }
}

size_t registeredAt(size_t position){
    const auto &apps=WindowManager::instance().apps();
    if(position<menuOrder.count)for(size_t i=0;i<apps.size();++i)if(apps[i].id==menuOrder.ids[position])return i;
    return apps.size();
}
size_t positionOf(size_t app){
    const auto &apps=WindowManager::instance().apps();
    return app<apps.size()?menuOrder.find(apps[app].id):menuOrder.count;
}
void openPosition(size_t position){if(!dragging&&!suppressClick)WindowManager::instance().openApp(registeredAt(position));}
void clearNotice(){if(noticeBox)lv_obj_delete(noticeBox);noticeBox=noticeLabel=nullptr;noticeUntil=0;}
void notice(const char *text,uint32_t duration=1500){
    if(!homeParent)return;
    if(!noticeBox){
        noticeBox=compact::panel(homeParent,228,26);if(!noticeBox)return;
        lv_obj_set_pos(noticeBox,6,0);lv_obj_set_clickable(noticeBox,false);
        lv_obj_set_style_bg_color(noticeBox,theme::surface(),0);
        lv_obj_set_style_border_width(noticeBox,1,0);lv_obj_set_style_border_color(noticeBox,theme::accent(),0);
        noticeLabel=compact::label(noticeBox,"",220);lv_obj_center(noticeLabel);
        lv_obj_set_style_text_align(noticeLabel,LV_TEXT_ALIGN_CENTER,0);
    }
    compact::setText(noticeLabel,text);lv_obj_move_foreground(noticeBox);
    noticeUntil=duration?lv_tick_get()+duration:0;
}
void translateX(void *obj,int32_t value){lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(obj),value,0);}
void translateY(void *obj,int32_t value){lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj),value,0);}
void stopShift(lv_obj_t *obj){
    lv_anim_delete(obj,translateX);lv_anim_delete(obj,translateY);
    translateX(obj,0);translateY(obj,0);
}
void shift(lv_obj_t *obj,int x,int y){
    auto animate=[&](lv_anim_exec_xcb_t cb,int start){
        if(!start)return;
        cb(obj,start);lv_anim_t anim;lv_anim_init(&anim);lv_anim_set_var(&anim,obj);
        lv_anim_set_exec_cb(&anim,cb);lv_anim_set_values(&anim,start,0);lv_anim_set_duration(&anim,130);
        lv_anim_set_path_cb(&anim,lv_anim_path_ease_out);lv_anim_start(&anim);
    };
    animate(translateX,x);animate(translateY,y);
}
void refreshRows(){
    if(!theme::terminal()||arrangePage)return;
    terminalRows.clear();
    for(size_t i=0;i<menuRows.size();++i){
        terminalRows.push_back(menuRows[i].object);
        if(menuRows[i].number)lv_label_set_text_fmt(menuRows[i].number,"%02u",unsigned(i+1));
    }
}
void insertRow(size_t from,size_t to,bool animate){
    if(from==to||from>=menuRows.size()||to>=menuRows.size())return;
    std::array<lv_point_t,orderModel::Capacity> old{};
    for(size_t i=0;i<menuRows.size();++i){
        auto obj=menuRows[i].object;stopShift(obj);lv_area_t a;lv_obj_get_coords(obj,&a);old[menuRows[i].app]={a.x1,a.y1};
    }
    auto row=menuRows[from];
    if(from<to)std::rotate(menuRows.begin()+from,menuRows.begin()+from+1,menuRows.begin()+to+1);
    else std::rotate(menuRows.begin()+to,menuRows.begin()+from,menuRows.begin()+from+1);
    lv_obj_move_to_index(row.object,int(to));lv_obj_update_layout(menuList);
    if(animate)for(const auto &item:menuRows){
        if(item.object==pressRow)continue;
        lv_area_t a;lv_obj_get_coords(item.object,&a);
        shift(item.object,old[item.app].x-a.x1,old[item.app].y-a.y1);
    }
    refreshRows();
}
void finishDrag(bool commit){
    if(!dragging)return;
    const auto from=menuOrder.find(WindowManager::instance().apps()[draggedApp].id);
    if(!commit){
        const auto to=dragBefore.find(WindowManager::instance().apps()[draggedApp].id);
        insertRow(from,to,false);menuOrder=dragBefore;
    }
    for(const auto &row:menuRows)stopShift(row.object);
    if(pressRow){lv_obj_set_style_opa(pressRow,LV_OPA_COVER,0);lv_obj_remove_state(pressRow,LV_STATE_PRESSED);}
    if(ghost)lv_obj_delete(ghost);
    ghost=nullptr;
    lv_obj_set_scrollable(menuList,true);dragging=false;dragInput=nullptr;pressRow=nullptr;suppressClick=true;
    if(theme::terminal()&&!arrangePage){
        cursor=positionOf(draggedApp);
        for(size_t i=0;i<terminalRows.size();++i)compact::checked(terminalRows[i],i==cursor);
    }
    if(commit&&!(menuOrder==dragBefore)){
        if(sys::Settings::setAppOrder(menuOrder)){
            waitingSave=true;shownSave=orderModel::SaveState::Pending;notice("正在保存顺序",0);
        }else notice("无法保存顺序");
    }else clearNotice();
}
void dragTick(){
    if(!dragging||!menuList)return;
    lv_indev_get_point(dragInput,&dragPoint);
    lv_area_t area;lv_obj_get_coords(menuList,&area);
    lv_area_t parentArea;lv_obj_get_coords(homeParent,&parentArea);
    const int width=lv_obj_get_width(ghost),height=lv_obj_get_height(ghost);
    lv_obj_set_pos(ghost,std::clamp(int(dragPoint.x-grabOffset.x-parentArea.x1),0,240-width),
        std::clamp(int(dragPoint.y-grabOffset.y-parentArea.y1),0,int(lv_obj_get_height(homeParent))-height));
    const auto now=lv_tick_get();if(int32_t(now-nextDragTick)<0)return;nextDragTick=now+30;
    const int edge=24;
    int step=0;
    if(dragPoint.y<area.y1+edge)step=-std::clamp(int((area.y1+edge-dragPoint.y)/3),1,8);
    else if(dragPoint.y>area.y2-edge)step=std::clamp(int((dragPoint.y-area.y2+edge)/3),1,8);
    const int rowGap = (menuColumns == 2) ? 6 : theme::kGap;
    const int total=int((menuRows.size()+menuColumns-1)/menuColumns)*(menuRowHeight+rowGap)-rowGap;
    const int maxScroll=std::max(0,total-int(lv_obj_get_height(menuList)));
    if(step)lv_obj_scroll_to_y(menuList,std::clamp(int(lv_obj_get_scroll_y(menuList))+step,0,maxScroll),LV_ANIM_OFF);
    if(dragPoint.x<area.x1||dragPoint.x>area.x2)return;
    const int y=std::clamp(int(dragPoint.y),int(area.y1),int(area.y2))-area.y1+lv_obj_get_scroll_y(menuList);
    const int col=menuColumns==2&&dragPoint.x>=area.x1+120?1:0;
    size_t to=std::min(size_t(std::max(0,y)/(menuRowHeight+rowGap)*int(menuColumns)+col),menuRows.size()-1);
    if(menuOrder.ids[to]==orderModel::Settings){if(!to)return;--to;}
    const auto from=positionOf(draggedApp);
    if(orderModel::move(menuOrder,from,to))insertRow(from,to,true);
}
void beginDrag(lv_obj_t *obj,lv_indev_t *input){
    if(!input||lv_indev_get_type(input)!=LV_INDEV_TYPE_POINTER||pressMoved||lv_indev_get_scroll_obj(input)||menuRows.size()<2)return;
    auto found=std::find_if(menuRows.begin(),menuRows.end(),[&](const MenuRow &row){return row.object==obj;});
    if(found==menuRows.end())return;
    suppressClick=true;
    const auto &app=WindowManager::instance().apps()[found->app];
    if(app.id==orderModel::Settings){notice("设置固定在最后");return;}
    ghost=compact::panel(homeParent,lv_obj_get_width(obj),lv_obj_get_height(obj));if(!ghost)return;
    lv_obj_set_clickable(ghost,false);lv_obj_set_style_radius(ghost,14,0);
    lv_obj_set_style_bg_color(ghost,theme::surface(),0);
    lv_obj_set_style_border_width(ghost,2,0);lv_obj_set_style_border_color(ghost,theme::accent(),0);
    
    auto badge=lv_obj_create(ghost);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge,34,34);
    lv_obj_align(badge,LV_ALIGN_LEFT_MID,6,0);
    lv_obj_set_style_radius(badge,10,0);
    lv_obj_set_style_bg_color(badge,theme::accent(),0);
    lv_obj_set_style_bg_opa(badge,LV_OPA_COVER,0);

    auto icon=compact::label(badge,app.icon,32);lv_obj_set_style_text_font(icon,icons::font24(),0);
    lv_obj_set_style_text_color(icon,lv_color_hex(0xFFFFFF),0);lv_obj_center(icon);

    auto name=compact::label(ghost,app.name,60);lv_obj_set_style_text_font(name,theme::fontMenu(),0);
    lv_obj_set_style_text_color(name,theme::fg(),0);lv_obj_align(name,LV_ALIGN_LEFT_MID,46,0);
    lv_indev_get_point(input,&dragPoint);lv_area_t a;lv_obj_get_coords(obj,&a);
    grabOffset={dragPoint.x-a.x1,dragPoint.y-a.y1};dragInput=input;pressRow=obj;draggedApp=found->app;
    dragBefore=menuOrder;dragging=true;nextDragTick=0;
    lv_obj_set_scrollable(menuList,false);lv_obj_set_style_opa(obj,LV_OPA_30,0);
    notice("拖动调整 · 松手保存",0);lv_obj_move_foreground(ghost);dragTick();
}
void menuEvent(lv_event_t *event){
    auto obj=lv_event_get_target_obj(event);const auto code=lv_event_get_code(event);auto input=lv_indev_active();
    if(code==LV_EVENT_PRESSED){
        suppressClick=false;pressMoved=false;swipeHandled=false;pressRow=obj;
        if(input)lv_indev_get_point(input,&pressPoint);
    }
    if(code==LV_EVENT_PRESSING&&input&&!dragging){
        lv_point_t point;lv_indev_get_point(input,&point);
        const int dx=point.x-pressPoint.x,dy=point.y-pressPoint.y;
        if(std::abs(dx)>8||std::abs(dy)>8)pressMoved=true;
        checkCandySwipe(dx,dy,30);
    }
    if(code==LV_EVENT_LONG_PRESSED&&!dragging&&!swipeHandled)beginDrag(obj,input);
    if(code==LV_EVENT_RELEASED){
        if(dragging&&obj==pressRow)finishDrag(true);
        else if(input&&!dragging&&!swipeHandled){
            lv_point_t point;lv_indev_get_point(input,&point);
            checkCandySwipe(point.x-pressPoint.x,point.y-pressPoint.y,26);
        }
    }
    if(code==LV_EVENT_PRESS_LOST){pressMoved=suppressClick=true;if(dragging)finishDrag(false);}
}
void bindRow(lv_obj_t *obj,size_t app,lv_obj_t *number=nullptr){
    menuRows.push_back({obj,app,number});lv_obj_set_press_lock(obj,true);lv_obj_set_gesture_bubble(obj,false);
    lv_obj_add_event_cb(obj,menuEvent,LV_EVENT_ALL,nullptr);
}
size_t appIndex(const char *name) {
    const auto &apps=WindowManager::instance().apps();
    for(size_t i=0;i<apps.size();++i) if(std::strcmp(apps[i].name,name)==0) return i;
    return apps.size();
}
void openNamed(lv_event_t *event) {
    if(suppressClick||dragging||pressMoved||swipeHandled)return;
    WindowManager::instance().openApp(appIndex(static_cast<const char *>(lv_event_get_user_data(event))));
}
void setCursor(size_t index) {
    if(terminalRows.empty()) return;
    cursor=index%terminalRows.size();
    for(size_t i=0;i<terminalRows.size();++i) compact::checked(terminalRows[i],i==cursor);
    lv_obj_scroll_to_view(terminalRows[cursor],LV_ANIM_OFF);
}
void terminalClick(lv_event_t *e) {
    if(suppressClick||dragging||pressMoved)return;
    const auto index=positionOf(static_cast<size_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
    if(index==cursor) openPosition(index);
    else setCursor(index);
}
void buildTerminal(lv_obj_t *parent) {
    auto list=compact::panel(parent,240,179,true);
    menuList=list;menuColumns=1;menuRowHeight=44;
    lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
    const auto &apps=WindowManager::instance().apps();
    terminalRows.reserve(apps.size());
    for(size_t position=0;position<menuOrder.count;++position) {
        const auto i=registeredAt(position);
        auto row=compact::button(list,"",terminalClick,reinterpret_cast<void *>(i),240,44);
        terminalRows.push_back(row);
        char number[12];std::snprintf(number,sizeof(number),"%02u",static_cast<unsigned>(position+1));
        auto num=compact::label(row,number,28);lv_obj_align(num,LV_ALIGN_LEFT_MID,5,0);
        auto icon=compact::label(row,apps[i].icon,32);lv_obj_set_style_text_font(icon,icons::font24(),0);
        lv_obj_align(icon,LV_ALIGN_LEFT_MID,38,0);
        auto title=compact::label(row,apps[i].name,137);lv_obj_align(title,LV_ALIGN_LEFT_MID,78,0);
        auto arrow=compact::label(row,LV_SYMBOL_RIGHT);lv_obj_align(arrow,LV_ALIGN_RIGHT_MID,-5,0);
        bindRow(row,i,num);
    }
    auto dock=compact::panel(parent,240,36);lv_obj_set_pos(dock,0,180);
    auto prev=compact::button(dock,LV_SYMBOL_UP,[](lv_event_t *) {
        if(!terminalRows.empty())setCursor((cursor+terminalRows.size()-1)%terminalRows.size());
    },nullptr,59,36);
    auto enter=compact::button(dock,"打开",[](lv_event_t *) { if(!dragging)WindowManager::instance().openApp(registeredAt(cursor)); },nullptr,120,36);
    auto next=compact::button(dock,LV_SYMBOL_DOWN,[](lv_event_t *) {setCursor(cursor+1);},nullptr,59,36);
    lv_obj_set_pos(prev,0,0);lv_obj_set_pos(enter,60,0);lv_obj_set_pos(next,181,0);
    setCursor(0);
}
void updateTiles(lv_timer_t *) {
    if(!sys::Power::isScreenOn() || !WindowManager::instance().atHome() || !desktop || lv_obj_is_hidden(desktop)) return;
    const auto now=time(nullptr);tm local{};localtime_r(&now,&local);
    char text[40];
    if(candyMode) {
        if(homeHour && homeMin) {
            std::snprintf(text, sizeof(text), "%02d", local.tm_hour);
            compact::setText(homeHour, text);
            std::snprintf(text, sizeof(text), "%02d", local.tm_min);
            compact::setText(homeMin, text);
        }

        if(homeDate) {
            namespace cal = calendar;
            const cal::Date d{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
            const auto l = cal::lunar(d);
            const char *fest = cal::festival(d);
            const char *term = cal::term(d);
            const bool isWeekend = (local.tm_wday == 0 || local.tm_wday == 6);

            static const char * const kWeekdaysZh[] = {
                "周日", "周一", "周二", "周三", "周四", "周五", "周六"
            };
            const char *weekStr = kWeekdaysZh[local.tm_wday % 7];

            const char *extra = "";
            if(fest && *fest) {
                extra = fest;
            } else if(term && *term) {
                extra = term;
            } else if(l && l.day >= 1 && l.day <= 30) {
                extra = cal::Days[l.day - 1];
            }

            char dateBuf[48];
            if(extra && *extra) {
                std::snprintf(dateBuf, sizeof(dateBuf), "%02d月%02d日 %s · %s",
                    local.tm_mon + 1, local.tm_mday, weekStr, extra);
            } else {
                std::snprintf(dateBuf, sizeof(dateBuf), "%02d月%02d日 %s",
                    local.tm_mon + 1, local.tm_mday, weekStr);
            }
            compact::setText(homeDate, dateBuf);

            if(homePill) {
                if(isWeekend) {
                    lv_obj_set_style_bg_color(homePill, lv_color_hex(0xFFF0F4), 0);
                    lv_obj_set_style_border_color(homePill, lv_color_hex(0xFFB8C6), 0);
                    lv_obj_set_style_text_color(homeDate, lv_color_hex(0xFF5A76), 0);
                } else {
                    lv_obj_set_style_bg_color(homePill, theme::surface(), 0);
                    lv_obj_set_style_border_color(homePill, lv_color_hex(0xFFD6E0), 0);
                    lv_obj_set_style_text_color(homeDate, theme::fg(), 0);
                }
            }
        }
    } else {
        if(homeTime) {
            std::snprintf(text,sizeof(text),"%02d:%02d",local.tm_hour,local.tm_min);
            compact::setText(homeTime,text);
        }
        if(homeDate) {
            std::snprintf(text,sizeof(text),"%04d.%02d.%02d",local.tm_year+1900,local.tm_mon+1,local.tm_mday);
            compact::setText(homeDate,text);
        }
    }
}
void tilePage(bool apps) {
    compact::checked(desktopTab,!apps);compact::checked(appsTab,apps);
    if(apps) {lv_obj_set_hidden(desktop,true);lv_obj_set_hidden(allApps,false);}
    else {lv_obj_set_hidden(allApps,true);lv_obj_set_hidden(desktop,false);updateTiles(nullptr);}
}
}

static void appItemCb(lv_event_t *e)
{
    if(suppressClick||dragging||pressMoved||swipeHandled||arrangePage)return;
    const size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
    WindowManager::instance().openApp(idx);
}

static constexpr uint32_t kCandyColors[] = {
    0xFF7D96, // 樱花草莓粉
    0xFFA940, // 甜橙奶冻
    0x36CFC9, // 薄荷苏打青
    0x73D13D, // 青苹果绿
    0x597EF7, // 晴朗天空蓝
    0xB37FEB, // 香芋软糖紫
    0xFFC069, // 芝士奶酪黄
    0xF759AB, // 蜜桃波波粉
};

static void buildAppList(lv_obj_t *parent)
{
    s_app_list = lv_obj_create(parent);
    constexpr int kPad = 6;
    constexpr int kColGap = 6;
    constexpr int kRowGap = 6;
    constexpr int kTileH = 56;
    menuList=s_app_list;menuColumns=2;menuRowHeight=kTileH;
    lv_obj_set_size(s_app_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_app_list, theme::bg(), 0);
    lv_obj_set_style_bg_opa(s_app_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_app_list, 0, 0);
    
    // 增加舒适的 padding 和 gap，呈现出糖果小卡片阵列
    lv_obj_set_style_pad_hor(s_app_list, kPad, 0);
    lv_obj_set_style_pad_top(s_app_list, kPad, 0);
    lv_obj_set_style_pad_bottom(s_app_list, kPad + 8, 0);
    lv_obj_set_style_pad_row(s_app_list, kRowGap, 0);
    lv_obj_set_style_pad_column(s_app_list, kColGap, 0);
    lv_obj_set_flex_flow(s_app_list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(s_app_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_app_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_app_list, candySwipeEvent, LV_EVENT_ALL, nullptr);

    // 宽度计算： (240 - 2 * 6 - 6) / 2 = 111px
    constexpr int kTileW = (theme::kScreenW - 2 * kPad - kColGap) / 2;

    const auto &apps = WindowManager::instance().apps();
    for (size_t position = 0; position < menuOrder.count; ++position) {
        const auto i=registeredAt(position);
        lv_obj_t *btn = compact::button(s_app_list,"",appItemCb,
            reinterpret_cast<void *>(static_cast<intptr_t>(i)),kTileW,kTileH);
        lv_obj_set_flex_grow(btn,0);
        lv_obj_set_style_pad_all(btn,0,0);
        lv_obj_set_style_radius(btn,14,0); // 14px Q弹大圆角
        lv_obj_set_style_bg_color(btn,theme::surface(),0);
        lv_obj_set_style_border_width(btn,1,0);
        lv_obj_set_style_border_color(btn,theme::edge(),0);
        lv_obj_set_style_outline_width(btn,0,0);

        // 彩色糖果图标徽章底托 (Candy Badge)
        lv_obj_t *badge = lv_obj_create(btn);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge,34,34);
        lv_obj_align(badge,LV_ALIGN_LEFT_MID,6,0);
        lv_obj_set_style_radius(badge,10,0); // 萌萌小圆角
        lv_obj_set_style_bg_color(badge,lv_color_hex(kCandyColors[position % (sizeof(kCandyColors)/sizeof(kCandyColors[0]))]),0);
        lv_obj_set_style_bg_opa(badge,LV_OPA_COVER,0);
        lv_obj_set_scrollable(badge,false);
        lv_obj_remove_flag(badge,LV_OBJ_FLAG_CLICKABLE);

        // 图标居中于糖果徽章中，白色纯净高亮
        lv_obj_t *icon = lv_label_create(badge);
        lv_label_set_text(icon, apps[i].icon ? apps[i].icon : "");
        lv_obj_set_style_text_font(icon, icons::font24(), 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon);
        lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE);

        // 中文应用名称
        lv_obj_t *name = lv_label_create(btn);
        lv_label_set_text(name, apps[i].name);
        lv_obj_set_style_text_font(name, theme::fontMenu(), 0);
        lv_obj_set_style_text_color(name, theme::fg(), 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(name, kTileW - 46 - 4);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 46, 0);
        lv_obj_remove_flag(name,LV_OBJ_FLAG_CLICKABLE);

        bindRow(btn,i);
    }
}

static void buildArrange(){
    arrangePending=false;
    if(arrangePage)return;
    focusSelectedId=menuOrder.ids[focusApps.selected()];
    arrangePage=compact::panel(homeParent,240,216);
    auto title=compact::label(arrangePage,"长按拖动排序",172);lv_obj_set_pos(title,5,7);
    auto done=compact::button(arrangePage,"完成",[](lv_event_t *){closeArrangePending=true;},nullptr,58,34);
    lv_obj_set_pos(done,181,0);
    auto content=compact::panel(arrangePage,240,180);lv_obj_set_pos(content,0,36);
    buildAppList(content);lv_obj_update_layout(content);
    if(focusApps.selected()<menuRows.size())lv_obj_scroll_to_view(menuRows[focusApps.selected()].object,LV_ANIM_OFF);
}

static void buildCandyHome(lv_obj_t *parent) {
    candyMode = true;
    candyAppsPage = false;

    // 启动/确保重力感应已开启，并获取临时访问租约
    if(!sys::Settings::motionSensorOn()) {
        sys::Settings::setMotionSensorOn(true);
    }
    s_imuLease = sys::Qmi8658::claimTemporaryUse();

    // Page 0: 主屏 (240x198)
    desktop = compact::panel(parent, 240, 198);
    lv_obj_set_pos(desktop, 0, 0);
    lv_obj_set_style_bg_color(desktop, theme::bg(), 0);
    lv_obj_set_style_border_width(desktop, 0, 0);
    lv_obj_set_clickable(desktop, true);
    lv_obj_add_event_cb(desktop, candySwipeEvent, LV_EVENT_ALL, nullptr);

    // 上半屏：重力感应草莓波波摇摇乐 (228x114)
    auto shakeCup = compact::panel(desktop, 228, 114);
    lv_obj_set_pos(shakeCup, 6, 2);
    lv_obj_set_style_pad_all(shakeCup, 0, 0);
    lv_obj_set_style_radius(shakeCup, 20, 0);
    lv_obj_set_style_bg_color(shakeCup, lv_color_hex(0xFFF7FA), 0);
    lv_obj_set_style_bg_opa(shakeCup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(shakeCup, 2, 0);
    lv_obj_set_style_border_color(shakeCup, lv_color_hex(0xFFC0CE), 0);
    lv_obj_set_clickable(shakeCup, true);
    lv_obj_add_event_cb(shakeCup, candySwipeEvent, LV_EVENT_ALL, nullptr);
    lv_obj_add_event_cb(shakeCup, [](lv_event_t *) {
        if(!suppressClick && !dragging && !pressMoved && !swipeHandled) {
            popBalls(1.0f);
        }
    }, LV_EVENT_CLICKED, nullptr);

    // 1. 顶部半透明标题胶囊药丸 (172x24)
    auto cupBadge = compact::panel(shakeCup, 172, 24);
    lv_obj_align(cupBadge, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_pad_all(cupBadge, 0, 0);
    lv_obj_set_style_radius(cupBadge, 12, 0);
    lv_obj_set_style_bg_color(cupBadge, lv_color_hex(0xFFEBF0), 0);
    lv_obj_set_style_bg_opa(cupBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cupBadge, 1, 0);
    lv_obj_set_style_border_color(cupBadge, lv_color_hex(0xFFCCD6), 0);
    lv_obj_set_clickable(cupBadge, true);
    lv_obj_add_event_cb(cupBadge, openNamed, LV_EVENT_CLICKED, const_cast<char *>("趣味随机"));

    // 左侧小草莓矢量微图标
    auto bStraw = lv_obj_create(cupBadge);
    lv_obj_remove_style_all(bStraw);
    lv_obj_set_size(bStraw, 10, 10);
    lv_obj_align(bStraw, LV_ALIGN_LEFT_MID, 10, 1);
    lv_obj_set_style_radius(bStraw, 5, 0);
    lv_obj_set_style_bg_color(bStraw, lv_color_hex(0xFF5A76), 0);
    lv_obj_set_style_bg_opa(bStraw, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bStraw, LV_OBJ_FLAG_CLICKABLE);

    auto bLeaf = lv_obj_create(bStraw);
    lv_obj_remove_style_all(bLeaf);
    lv_obj_set_size(bLeaf, 4, 3);
    lv_obj_set_pos(bLeaf, 3, 0);
    lv_obj_set_style_radius(bLeaf, 1, 0);
    lv_obj_set_style_bg_color(bLeaf, lv_color_hex(0x52C41A), 0);
    lv_obj_set_style_bg_opa(bLeaf, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bLeaf, LV_OBJ_FLAG_CLICKABLE);

    // 中间纯净中文标题 (不再包含外部 emoji，绝对不会乱码方块)
    auto badgeLabel = lv_label_create(cupBadge);
    lv_label_set_text(badgeLabel, "草莓波波摇摇乐");
    lv_obj_set_style_text_font(badgeLabel, theme::fontCjkFull(), 0);
    lv_obj_set_style_text_color(badgeLabel, lv_color_hex(0xFF5A76), 0);
    lv_obj_center(badgeLabel);
    lv_obj_remove_flag(badgeLabel, LV_OBJ_FLAG_CLICKABLE);

    // 右侧小猫爪矢量微图标
    auto bPaw = lv_obj_create(cupBadge);
    lv_obj_remove_style_all(bPaw);
    lv_obj_set_size(bPaw, 10, 8);
    lv_obj_align(bPaw, LV_ALIGN_RIGHT_MID, -10, 1);
    lv_obj_set_style_radius(bPaw, 4, 0);
    lv_obj_set_style_bg_color(bPaw, lv_color_hex(0xFF8DA1), 0);
    lv_obj_set_style_bg_opa(bPaw, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bPaw, LV_OBJ_FLAG_CLICKABLE);

    // 2. 构建 6 颗具有真实重力与弹力碰撞的萌萌波波球 (增大尺寸，强化Q弹视觉)
    struct BallConfig {
        float x, y, r, mass;
        uint32_t color;
    };
    static const BallConfig kConfigs[kNumBalls] = {
        {  34.0f,  88.0f, 15.0f, 1.4f, 0xFF5A76 }, // 草莓大波波 (红)
        {  68.0f,  90.0f, 14.0f, 1.3f, 0x4E342E }, // 软萌猫爪球 (可可黑糖)
        { 100.0f,  88.0f, 14.0f, 1.3f, 0xFFA439 }, // 芒果星星球 (金黄)
        { 132.0f,  90.0f, 14.0f, 1.3f, 0xFF7D96 }, // 蜜桃甜心球 (草莓粉)
        { 164.0f,  88.0f, 13.0f, 1.2f, 0x9F7AEA }, // 香芋软波波 (香芋紫)
        { 194.0f,  90.0f, 13.0f, 1.2f, 0x36CFC9 }, // 薄荷苏打球 (薄荷青)
    };

    for(int i = 0; i < kNumBalls; ++i) {
        const auto &cfg = kConfigs[i];
        s_balls[i].x = cfg.x;
        s_balls[i].y = cfg.y;
        s_balls[i].vx = 0.0f;
        s_balls[i].vy = 0.0f;
        s_balls[i].r = cfg.r;
        s_balls[i].mass = cfg.mass;

        const int diam = int(cfg.r * 2.0f);
        auto ball = lv_obj_create(shakeCup);
        lv_obj_remove_style_all(ball);
        lv_obj_set_size(ball, diam, diam);
        lv_obj_set_pos(ball, int(cfg.x - cfg.r), int(cfg.y - cfg.r));
        lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(ball, lv_color_hex(cfg.color), 0);
        lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ball, 2, 0);
        lv_obj_set_style_border_color(ball, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_scrollable(ball, false);
        lv_obj_remove_flag(ball, LV_OBJ_FLAG_CLICKABLE);
        s_balls[i].obj = ball;

        // 每颗球的专属萌系装饰图案
        if(i == 0) {
            // 草莓大波波：绿顶草莓叶片 + 晶莹高光点
            auto leaf = lv_obj_create(ball);
            lv_obj_remove_style_all(leaf);
            lv_obj_set_size(leaf, 10, 5);
            lv_obj_set_pos(leaf, int(cfg.r - 5.0f), 2);
            lv_obj_set_style_radius(leaf, 2, 0);
            lv_obj_set_style_bg_color(leaf, lv_color_hex(0x52C41A), 0);
            lv_obj_set_style_bg_opa(leaf, LV_OPA_COVER, 0);
            lv_obj_remove_flag(leaf, LV_OBJ_FLAG_CLICKABLE);

            auto gloss = lv_obj_create(ball);
            lv_obj_remove_style_all(gloss);
            lv_obj_set_size(gloss, 5, 5);
            lv_obj_set_pos(gloss, 6, 6);
            lv_obj_set_style_radius(gloss, 2, 0);
            lv_obj_set_style_bg_color(gloss, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(gloss, LV_OPA_80, 0);
            lv_obj_remove_flag(gloss, LV_OBJ_FLAG_CLICKABLE);
        } else if(i == 1) {
            // 可可猫爪球：粉嫩大肉垫 + 3颗猫爪小豆豆
            auto pad = lv_obj_create(ball);
            lv_obj_remove_style_all(pad);
            lv_obj_set_size(pad, 10, 7);
            lv_obj_set_pos(pad, int(cfg.r - 5.0f), 13);
            lv_obj_set_style_radius(pad, 3, 0);
            lv_obj_set_style_bg_color(pad, lv_color_hex(0xFF8DA1), 0);
            lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
            lv_obj_remove_flag(pad, LV_OBJ_FLAG_CLICKABLE);

            static constexpr int kToesX[3] = { 5, 10, 15 };
            static constexpr int kToesY[3] = { 7, 5, 7 };
            for(int t = 0; t < 3; ++t) {
                auto toe = lv_obj_create(ball);
                lv_obj_remove_style_all(toe);
                lv_obj_set_size(toe, 3, 3);
                lv_obj_set_pos(toe, kToesX[t], kToesY[t]);
                lv_obj_set_style_radius(toe, 1, 0);
                lv_obj_set_style_bg_color(toe, lv_color_hex(0xFF8DA1), 0);
                lv_obj_set_style_bg_opa(toe, LV_OPA_COVER, 0);
                lv_obj_remove_flag(toe, LV_OBJ_FLAG_CLICKABLE);
            }
        } else if(i == 2) {
            // 芒果星星球：白色星光核
            auto starCore = lv_obj_create(ball);
            lv_obj_remove_style_all(starCore);
            lv_obj_set_size(starCore, 8, 8);
            lv_obj_center(starCore);
            lv_obj_set_style_radius(starCore, 3, 0);
            lv_obj_set_style_bg_color(starCore, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(starCore, LV_OPA_90, 0);
            lv_obj_remove_flag(starCore, LV_OBJ_FLAG_CLICKABLE);
        } else if(i == 3) {
            // 蜜桃甜心球：奶白柔光甜心
            auto heartCore = lv_obj_create(ball);
            lv_obj_remove_style_all(heartCore);
            lv_obj_set_size(heartCore, 8, 8);
            lv_obj_center(heartCore);
            lv_obj_set_style_radius(heartCore, 4, 0);
            lv_obj_set_style_bg_color(heartCore, lv_color_hex(0xFFF0F4), 0);
            lv_obj_set_style_bg_opa(heartCore, LV_OPA_COVER, 0);
            lv_obj_remove_flag(heartCore, LV_OBJ_FLAG_CLICKABLE);
        } else if(i == 4) {
            // 香芋波波球：淡紫高光
            auto taroDot = lv_obj_create(ball);
            lv_obj_remove_style_all(taroDot);
            lv_obj_set_size(taroDot, 6, 6);
            lv_obj_center(taroDot);
            lv_obj_set_style_radius(taroDot, 3, 0);
            lv_obj_set_style_bg_color(taroDot, lv_color_hex(0xEDE9FE), 0);
            lv_obj_set_style_bg_opa(taroDot, LV_OPA_COVER, 0);
            lv_obj_remove_flag(taroDot, LV_OBJ_FLAG_CLICKABLE);
        } else if(i == 5) {
            // 薄荷苏打球：微气泡水光
            auto bubble = lv_obj_create(ball);
            lv_obj_remove_style_all(bubble);
            lv_obj_set_size(bubble, 5, 5);
            lv_obj_set_pos(bubble, 5, 5);
            lv_obj_set_style_radius(bubble, 2, 0);
            lv_obj_set_style_bg_color(bubble, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
            lv_obj_remove_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // 启动 30Hz (~33ms) 物理引擎计算定时器
    s_physicsTimer = lv_timer_create(physicsTick, 33, nullptr);

    // 下半屏：3 个高频快捷大糖果按键（小智AI、小恐龙、萌芽物语）
    struct ShortcutSpec {
        const char *name;
        const char *icon;
        uint32_t color;
    };
    static constexpr ShortcutSpec kShortcuts[3] = {
        {"小智AI", icons::Xiaozhi, 0xFF7D96},
        {"小恐龙", icons::Dino,    0x9F7AEA},
        {"萌芽物语", icons::Garden, 0x36CFC9},
    };
    for(int idx = 0; idx < 3; ++idx) {
        auto btn = compact::button(desktop, "", openNamed,
            const_cast<char *>(kShortcuts[idx].name), 72, 74);
        lv_obj_set_pos(btn, 6 + idx * 78, 120);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_set_style_bg_color(btn, theme::surface(), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xFFD6E0), 0);
        lv_obj_set_style_outline_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, candySwipeEvent, LV_EVENT_ALL, nullptr);

        auto badge = lv_obj_create(btn);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge, 36, 36);
        lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 7);
        lv_obj_set_style_radius(badge, 12, 0);
        lv_obj_set_style_bg_color(badge, lv_color_hex(kShortcuts[idx].color), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);

        auto icon = lv_label_create(badge);
        lv_label_set_text(icon, kShortcuts[idx].icon);
        lv_obj_set_style_text_font(icon, icons::font24(), 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

        auto lbl = lv_label_create(btn);
        lv_label_set_text(lbl, kShortcuts[idx].name);
        lv_obj_set_style_text_font(lbl, theme::fontMenu(), 0);
        lv_obj_set_style_text_color(lbl, theme::fg(), 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -7);
        lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    }

    // Page 1: 左滑双列糖果应用列表页 (240x198)
    allApps = compact::panel(parent, 240, 198);
    lv_obj_set_pos(allApps, 0, 0);
    lv_obj_set_style_bg_color(allApps, theme::bg(), 0);
    lv_obj_set_style_border_width(allApps, 0, 0);
    lv_obj_add_event_cb(allApps, candySwipeEvent, LV_EVENT_ALL, nullptr);
    buildAppList(allApps);

    // 底部翻页指示点栏 (240x18)，点击也可快速切换主屏/应用列表
    candyDock = compact::panel(parent, 240, 18);
    lv_obj_set_pos(candyDock, 0, 198);
    lv_obj_set_style_bg_color(candyDock, theme::bg(), 0);
    lv_obj_set_style_border_width(candyDock, 0, 0);
    lv_obj_set_clickable(candyDock, true);
    lv_obj_add_event_cb(candyDock, [](lv_event_t *) {
        if(!dragging && !pressMoved && !swipeHandled) {
            candySetPage(!candyAppsPage, true);
        }
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(candyDock, candySwipeEvent, LV_EVENT_ALL, nullptr);

    candyDot0 = lv_obj_create(candyDock);
    lv_obj_remove_style_all(candyDot0);
    lv_obj_set_style_radius(candyDot0, 3, 0);
    lv_obj_set_style_bg_opa(candyDot0, LV_OPA_COVER, 0);
    lv_obj_remove_flag(candyDot0, LV_OBJ_FLAG_CLICKABLE);

    candyDot1 = lv_obj_create(candyDock);
    lv_obj_remove_style_all(candyDot1);
    lv_obj_set_style_radius(candyDot1, 3, 0);
    lv_obj_set_style_bg_opa(candyDot1, LV_OPA_COVER, 0);
    lv_obj_remove_flag(candyDot1, LV_OBJ_FLAG_CLICKABLE);

    // 默认开机进入主屏 (Page 0)
    candyAppsPage = true; // 确保首次调用 candySetPage(false) 完整刷新状态
    candySetPage(false, false);
    tileTimer = lv_timer_create(updateTiles, 1000, nullptr);
}

static void buildTiles(lv_obj_t *parent) {
    candyMode = false;
    desktop=compact::panel(parent,240,179);
    allApps=compact::panel(parent,240,179);buildAppList(allApps);
    auto tile=[&](const char *name,int x,int y,int w,int h) {
        auto obj=compact::button(desktop,"",openNamed,const_cast<char *>(name),w,h);lv_obj_set_pos(obj,x,y);return obj;
    };
    auto clock=tile("时钟",0,0,143,87);
    compact::buttonBackground(clock,theme::accent());
    homeTime=compact::label(clock,"00:00",137);lv_obj_set_style_text_font(homeTime,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_align(homeTime,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_pos(homeTime,2,14);
    homeDate=compact::label(clock,"",137);lv_obj_set_style_text_font(homeDate,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_align(homeDate,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_pos(homeDate,2,59);
    auto rec=tile("录音机",144,0,96,43);auto recText=compact::label(rec,"录音机");lv_obj_center(recText);
    auto random=tile("趣味随机",144,44,96,43);auto randomText=compact::label(random,"趣味随机");lv_obj_center(randomText);
    auto vault=tile("小智AI",0,88,143,90);
    auto key=compact::label(vault,icons::Xiaozhi);lv_obj_set_style_text_font(key,icons::font24(),0);lv_obj_align(key,LV_ALIGN_TOP_MID,0,10);
    auto vaultText=compact::label(vault,"小智AI");lv_obj_align(vaultText,LV_ALIGN_BOTTOM_MID,0,-10);
    auto hid=tile("小恐龙",144,88,96,90);
    auto keyboardIcon=compact::label(hid,icons::Dino);lv_obj_set_style_text_font(keyboardIcon,icons::font24(),0);lv_obj_align(keyboardIcon,LV_ALIGN_TOP_MID,0,10);
    auto hidText=compact::label(hid,"小恐龙",90);
    lv_obj_set_style_text_font(hidText,theme::fontMenu(),0);
    lv_obj_set_style_text_align(hidText,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(hidText,LV_ALIGN_BOTTOM_MID,0,-10);
    auto dock=compact::panel(parent,240,36);lv_obj_set_pos(dock,0,180);
    auto home=compact::button(dock,"桌面",[](lv_event_t *){tilePage(false);},nullptr,79,36);
    auto apps=compact::button(dock,"应用",[](lv_event_t *){tilePage(true);},nullptr,79,36);
    auto settings=compact::button(dock,"设置",openNamed,const_cast<char *>("设置"),80,36);
    lv_obj_set_pos(home,0,0);lv_obj_set_pos(apps,80,0);lv_obj_set_pos(settings,160,0);
    desktopTab=home;appsTab=apps;
    tilePage(false);
    // Home UI only, not a preloaded clock App. One cheap, visibility-gated tick.
    tileTimer=lv_timer_create(updateTiles,1000,nullptr);
}

void HomeScreen::build(lv_obj_t *parent)
{
    homeParent=parent;
    candyMode=false;candyAppsPage=false;swipeHandled=false;
    orderModel::Order registered;
    for(const auto &app:WindowManager::instance().apps())if(registered.count<orderModel::Capacity)registered.ids[registered.count++]=app.id;
    menuOrder=orderModel::normalize(sys::Settings::appOrder(),registered);
    menuRows.reserve(menuOrder.count);
    lv_obj_add_event_cb(parent,[](lv_event_t *){
        if(tileTimer)lv_timer_delete(tileTimer);
        tileTimer=nullptr;
        if(s_physicsTimer) {
            lv_timer_delete(s_physicsTimer);
            s_physicsTimer = nullptr;
        }
        s_isSnoozing = false;
        s_lastMotionTick = 0;
        if(s_imuLease != 0) {
            sys::Qmi8658::releaseTemporaryUse(s_imuLease);
            s_imuLease = 0;
        }
        for(int i = 0; i < kNumBalls; ++i) {
            s_balls[i].obj = nullptr;
        }
        dragging=arrangePending=closeArrangePending=waitingSave=false;
        candyMode=candyAppsPage=swipeHandled=false;
        homeParent=menuList=s_app_list=arrangePage=ghost=noticeBox=noticeLabel=pressRow=nullptr;dragInput=nullptr;
        desktop=allApps=homeTime=homeDate=homePill=homeHour=homeMin=homePaw=desktopTab=appsTab=candyDock=candyDot0=candyDot1=nullptr;
        std::vector<MenuRow>().swap(menuRows);std::vector<lv_obj_t *>().swap(terminalRows);
        focusApps.reset();
    },LV_EVENT_DELETE,nullptr);
    lv_obj_set_style_bg_color(parent, theme::bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    if(theme::focus()) {
        focusApps.create(parent,216,WindowManager::instance().apps().size(),0,nullptr,
            [](void *,size_t i) {const auto &app=WindowManager::instance().apps()[registeredAt(i)];focusApps.content(app.name,app.icon,"左右选择 · 长按排序");},
            [](void *,size_t i) {WindowManager::instance().openApp(registeredAt(i));});
        focusApps.onLongPress([](void *,size_t){arrangePending=true;if(auto input=lv_indev_active())lv_indev_wait_release(input);});
    }
    else if(theme::tiles()) buildTiles(parent);
    else if(theme::terminal()) buildTerminal(parent);
    else buildCandyHome(parent);
}

bool HomeScreen::arranging(){return dragging||arrangePending||arrangePage;}
bool HomeScreen::cancelArrange(){
    const bool active=arranging();arrangePending=closeArrangePending=false;
    if(dragging)finishDrag(false);
    if(arrangePage){
        const auto selected=menuOrder.find(focusSelectedId);
        lv_obj_delete(arrangePage);arrangePage=nullptr;menuList=s_app_list=pressRow=nullptr;menuRows.clear();
        focusApps.select(selected);
    }
    clearNotice();return active;
}
bool HomeScreen::backToMain(){
    if(candyMode && candyAppsPage){
        candySetPage(false, true);
        return true;
    }
    return false;
}
void HomeScreen::poll(){
    if(!homeParent)return;
    if(!sys::Power::isScreenOn()||sys::Power::sleepPending()||!WindowManager::instance().atHome()){
        cancelArrange();return;
    }
    if(closeArrangePending)cancelArrange();
    if(arrangePending)buildArrange();
    if(dragging){
        if(!dragInput||lv_indev_get_state(dragInput)!=LV_INDEV_STATE_PRESSED||lv_indev_search_obj(lv_layer_top(),&dragPoint))finishDrag(false);
        else dragTick();
    }
    if(waitingSave&&!dragging){
        const auto state=sys::Settings::appOrderSaveState();
        if(state!=shownSave){
            shownSave=state;
            if(state==orderModel::SaveState::Saved){notice("顺序已保存");waitingSave=false;}
            else if(state==orderModel::SaveState::Failed)notice("保存失败，正在重试",4000);
        }
    }
    if(noticeUntil&&int32_t(lv_tick_get()-noticeUntil)>=0)clearNotice();
}

}// namespace ui
