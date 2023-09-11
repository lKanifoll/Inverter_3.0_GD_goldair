/* Includes ------------------------------------------------------------------*/


#include "inverter.h"

#include "ClickButton.h"
#include <string.h>
#include "math.h"
#include "crc32.h"
#include "time.h"
#include "images.h"
#include "fonts.h"
#include <Pixels_PPI16.h> 
#include <Pixels_ILI9341.h> 
#include <stddef.h>     /* offsetof */
//#include <OpenWindowControl.h>
#include <wifi.h>
#define RTC_CLOCK_SOURCE_IRC40K 
#define BKP_VALUE    0x32F0
#define SETTINGSADDR           ((uint32_t)0x0801F000U)
uint16_t raw;

/* Private variables ---------------------------------------------------------*/
/*
extern ADC_HandleTypeDef hadc;
extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim14;
*/
/* Private function prototypes -----------------------------------------------*/
const char* _calendarPresetName[8] = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "C"};

typedef struct MenuItem
{
	uint32_t ID; // ��������� �� �������� ������
	uint16_t counts; //���������� ������� ������� ����/�������
	struct MenuItem* items;
	void (*prev)(void); //��������� �� ������� ������������� �� ������� �� left
	void (*next)(void); //��������� �� ������� ������������� �� ������� �� right
	void (*enter)(void); //��������� �� ������� ������������� �� ������� �� enter
	int8_t selected;
	struct MenuItem* parent;
} MenuItem_t;

static const RGB powerLevelColors[] = { RGB(255,255,255), RGB(255,70,70), RGB(255,0,0), RGB(200,0,0), RGB(160,0,0) };//{ RGB(255,255,255), RGB(255,242,90), RGB(235,147,79), RGB(227,109,79), RGB(224,86,74) };
static const RGB modeColors[] = {RGB(200,0,0), RGB(255,255,255), RGB(14,117,188), RGB(0,0,0)};

static MenuItem_t _modeOK = {999, 0, NULL, NULL, NULL, MenuBack }; // OK

// HeatMode menu
static MenuItem_t _modeMenu[] = {
	{11, 0, NULL, 					NULL, NULL, NULL }, // turbo
	{12, 0, NULL, 					TempMinus, TempPlus, NULL }, // comfort
	{13, 0, NULL, 					TempMinus, TempPlus, NULL }, // eco
};

// Power menu
static MenuItem_t _powerMenu[] = {
	{21, 0, NULL, 					NULL, NULL, NULL}, // Auto
	{22, 5, NULL, 					NULL, NULL, NULL}, // Custom
};

// Date&time menu
static MenuItem_t _datetimeMenu[] = {
	{411, 0, NULL, 					DateMinus, DatePlus, NULL }, // Date
	{412, 0, NULL, 					TimeMinus, TimePlus, NULL }, // Time
};

// Timer menu
static MenuItem_t _timerMenu[] = {
	{31, 0, NULL, 					On, Off, NULL }, // On/off
	{32, 0, NULL, 					TimeMinus, TimePlus, NULL }, // Time
};

// Display menu
static MenuItem_t _displayMenu[] = {
	{421, 0, NULL, 					On, Off, NULL }, // On/off
	{422, 0, NULL, 					On, Off, NULL }, // Time
};

// Connect menu
static MenuItem_t _connMenu[] = {
	{4431, 0, NULL, 					NULL, NULL, NULL }, // connect
	//{4432, 0, NULL, 					On, Off, NULL },        // Reset
};	
// Service menu
static MenuItem_t _serviceMenu[] = {
	{443, 1, _connMenu, 		NULL, NULL, NULL }, // Wifi
	{441, 0, NULL, 					On, Off, NULL }, // Reset
	{442, 0, NULL, 					NULL, NULL, MenuBack }, // Info
	
};

// Settings menu
static MenuItem_t _settingsMenu[] = {
	{6, 2, NULL, 			NULL, NULL, NULL }, // F or C
	{441, 0, NULL, 					On, Off, NULL  }, // Reset
	{442, 0, NULL, 					NULL, NULL, MenuBack  }, 		// info
};

static MenuItem_t _presetMenu = 
	{510, 24, NULL, 					CustomPrev, CustomNext, NULL };

static MenuItem_t _presetViewMenu = 
	{511, 0, NULL, 					NULL, NULL, NULL };

static MenuItem_t _selectModeMenu = 
	{530, 4, NULL, 					NULL, NULL, NULL };

// Program menu
static MenuItem_t _programMenu[] = {
	{51, 7, NULL, 					NULL, NULL, NULL }, // Setup
	{52, 0, NULL, 					On, Off, NULL }, // Calendar
	//{53, 24, NULL, 					CustomPrev, CustomNext, NULL }, // Custom day
};

// Main menu
static MenuItem_t _mainMenu[] = {
	{443, 0, NULL,  			On, Off, NULL }, //Wifi
	{2, 2, NULL, 			NULL, NULL, NULL }, // Power
	{3, 25, NULL, 					NULL, NULL, NULL }, // Timer
	{4, 3, _settingsMenu,		NULL, NULL, NULL }, // Settings
	{421, 0, NULL, 					On, Off, NULL }, // Bri
	{43, 0, NULL, 					On, Off, NULL  }, // sound
};

static MenuItem_t _menu = 
	{0, 6, _mainMenu, 			NULL, NULL, NULL};


struct MenuItem* old;
MenuItem_t* currentMenu = NULL;
uint32_t idleTimeout = 0; 

uint8_t _currentPower = 0;
uint8_t semistor_power = 0;
int16_t temp_current	= 0;
uint8_t power_level_auto = 0;	
uint8_t powerback_flag = 0;
uint8_t _currentPowerTicks = 20;
uint8_t temp_prev = 0;
uint8_t mode_prev = 0;
	
uint32_t current_time = 0;

uint16_t currentWorkMode;
uint8_t _eventTimer = 0;
uint8_t _blocked = 0;
uint32_t _durationClick = 0;
uint32_t _timeoutSaveFlash = 0;

uint8_t _error = 0;
uint8_t _error_fl = 0;
int16_t _xWifi;
uint32_t _timerBlink = 0;
uint32_t _timerStart = 0;
bool _blink = false;
//OpenWindowControl _windowOpened;
uint8_t power_current;
uint8_t open_window_temp_main_start;
uint8_t power_limit = 20;
uint8_t open_window_counter = 0;
uint8_t histeresis_low;
uint8_t histeresis_high;

uint8_t open_window_times = 5;

uint8_t power_step = 1;
bool window_is_opened = 0;
bool window_was_opened = 0;
bool in_histeresis = 0;
bool heat_from_cold = 1;
static float temp_steinhart;
bool refresh_system = false;
uint16_t timer_time_set = 0;
StateBrightness _stateBrightness = StateBrightness_ON;
//Wifi _wifi;
uint8_t wifi_status = 0;
uint32_t nextChangeLevel = 0;
uint32_t refrash_time = 0;
uint8_t btn_buff[2];

rtc_parameter_struct rtc_initpara;
rtc_alarm_struct  rtc_alarm;
rtc_parameter_struct rtc_init_param;

ClickButton _key_window(12);
ClickButton _key_power(11);
ClickButton _key_menu(10);
ClickButton _key_back(9);
ClickButton _key_down(7, 500, true);
ClickButton _key_up(6, 500, true);

Pixels pxs(240, 320);
struct DeviceSettings _settings;
struct tm _dateTime;
static struct OnOffSettings _onoffSet = { 0, 0};
static struct PresetSettings _presetSet = {0, 0};
static struct TemperatureSettings _tempConfig;



void drawRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, int16_t thikness)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setBackground(BG_COLOR);
	pxs.fillRoundRectangle(320/2 - (x)/2, 240/2 - (y)/2, (width), (height), 12);
	pxs.setColor(BG_COLOR);
	pxs.setBackground(MAIN_COLOR);
	pxs.fillRoundRectangle(320/2 - (x-thikness)/2, 240/2 - (y-thikness)/2, (width-thikness), (height-thikness), 10);
  pxs.setColor(MAIN_COLOR);
	pxs.setBackground(BG_COLOR);
}

void rtc_setup(void)
{

	rtc_deinit();
	rtc_initpara.rtc_factor_asyn = 0x7FU;
	rtc_initpara.rtc_factor_syn = 0xFFU;
	rtc_initpara.rtc_year = 0x23;
	rtc_initpara.rtc_day_of_week = RTC_WEDSDAY;
	rtc_initpara.rtc_month = RTC_JAN;
	rtc_initpara.rtc_date = 0x01;
	rtc_initpara.rtc_display_format = RTC_24HOUR;
	rtc_initpara.rtc_am_pm = 0;

	rtc_initpara.rtc_hour = 0x12;
		 
	rtc_initpara.rtc_minute = 0x00;

	rtc_initpara.rtc_second = 0x00;

	if(ERROR == rtc_init(&rtc_initpara)){    
		//while(1);
	}

}

void alarm_set(uint8_t minutes)
{
	  rtc_alarm_disable();

    rtc_alarm.rtc_alarm_mask = RTC_ALARM_DATE_MASK | RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_SECOND_MASK;// | RTC_ALARM_SECOND_MASK | RTC_ALARM_HOUR_MASK;// | RTC_ALARM_SECOND_MASK;//RTC_ALARM_HOUR_MASK;
    rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
    rtc_alarm.rtc_alarm_day = RTC_MONDAY | RTC_TUESDAY | RTC_WEDSDAY | RTC_THURSDAY | RTC_FRIDAY | RTC_SATURDAY | RTC_SUNDAY;
    //rtc_alarm.rtc_am_pm = RTC_AM;

    /* RTC alarm input */
    rtc_alarm.rtc_alarm_hour = 0x00;

    rtc_alarm.rtc_alarm_minute = 0x00;

    rtc_alarm.rtc_alarm_second = 0x00;
    
    /* RTC alarm configuration */
    rtc_alarm_config(&rtc_alarm);
    
    //rtc_interrupt_enable(RTC_INT_ALARM);  
    rtc_alarm_enable(); 
	  rtc_flag_clear(RTC_STAT_ALRM0F);
		delay_1ms(100);
}




void smooth_backlight(uint8_t mode)
{
	if(mode)
	{
		for(uint16_t i=0;i<(_stateBrightness ? 100:500);i+=5)
		{
			timer_channel_output_pulse_value_config(TIMER1,TIMER_CH_0,i);
			delay_1ms(2);	
		}	
	}
	else
	{
		for(uint16_t j=(_stateBrightness ? 100:500);j>0;j--)
		{
			timer_channel_output_pulse_value_config(TIMER1,TIMER_CH_0,j);
		}
		timer_channel_output_pulse_value_config(TIMER1,TIMER_CH_0,0);
	}
}

void SaveFlash()
{
	
	fmc_unlock();

	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_page_erase(SETTINGSADDR);
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	
	
	uint32_t Address = SETTINGSADDR;
	uint8_t* AddressSrc = (uint8_t*)&_settings;
	_settings.crc = crc32_1byte(AddressSrc, offsetof(DeviceSettings, crc), 0xFFFFFFFF);
	int count = sizeof(_settings)/4 ;
	while(count--){
		fmc_word_program(Address, *(__IO uint32_t *)AddressSrc);
		Address += 4U;
		AddressSrc += 4U;
		
	}
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_lock();
	
}

void DrawLeftRight()
{
	pxs.drawCompressedBitmap(12, 101, img_left_png_comp);
	pxs.drawCompressedBitmap(286, 101, img_right_png_comp);
}

void DrawMenu()
{
	if (currentMenu == NULL)
		return;
	
	if (currentMenu->items == NULL)
	{
		DrawEditParameter();
		return;
	}

	pxs.clear();
	DrawLeftRight();


		
	const uint8_t *icon = NULL;
	char* text = NULL;
	
	switch (currentMenu->items[currentMenu->selected].ID)
	{
		case 1:
			//drawRoundRect(92,84,92,84,14,6);
			//pxs.setColor(MAIN_COLOR);
			//pxs.setFont(FuturaBookC20a);
			int16_t width = pxs.getTextWidth("MODE");
			//pxs.print(320 / 2 - width / 2-1, 240/2 - 10, "MODE");		
			text = "Heat mode";
			break;
		case 2:
		  drawRoundRect(92,84,92,84,12,6);
			pxs.fillRectangle(131,120,10,30);
			pxs.fillRectangle(147,112,10,38);
		  pxs.fillRectangle(163,104,10,46);
			pxs.fillRectangle(179,96 ,10,54);
			text = "Power";
			break;
		case 3:
			icon = img_menu_timer_icon_png_comp;
			text = "Timer";
			break;
		case 4:
			icon = img_menu_setting_icon_png_comp;
			text = "Settings";
			break;
		case 5:
			//icon = img_menu_program_icon_png_comp;
			text = "Programme";
			break;
		case 6:
			pxs.setFont(FuturaMediumC57a);
			DrawTextAligment(155 - (pxs.getTextWidth("�F/�C") / 2), 240/2 - 35, pxs.getTextWidth("�F/�C"), pxs.getTextLineHeight(), "�F/�C", false, false);			
			//icon = img_menu_program_icon_png_comp;
			text = "Change units";
			break;
		case 11:
			//icon = img_menu_mode_comfort_png_comp;
			text = "Comfort";
			break;
		case 12:
			pxs.setColor(MAIN_COLOR);
			pxs.setBackground(BG_COLOR);
			pxs.fillOval(320/2-78/2,240/2-78/2,78,78);
			pxs.setColor(BG_COLOR);
			pxs.setBackground(MAIN_COLOR);
		  pxs.fillOval(320/2-10,240/2-50,60,60);
			pxs.fillRectangle(320/2, 70, 60,50);
			pxs.setColor(MAIN_COLOR);
			pxs.setBackground(BG_COLOR);
			//icon = img_menu_mode_eco_png_comp;
			text = "Eco";
			break;
		case 13:
			//icon = img_menu_mode_anti_png_comp;
			text = "Anti-frost";
			break;
		case 21:
			drawRoundRect(92,84,92,84,12,6);
			pxs.setColor(MAIN_COLOR);
			//pxs.setFont(FuturaMediumC53a);
			//width = pxs.getTextWidth("A");
			//pxs.print(320 / 2 - width / 2, 240/2 - 27, "A");		
			text = "Auto";
			break;
		case 22:
			//drawRoundRect(92,84,92,84,12,6);
			//pxs.setColor(MAIN_COLOR);
			//pxs.setFont(FuturaMediumC53a);
			//width = pxs.getTextWidth("C");
			//pxs.print(320 / 2 - width / 2-3, 240/2 - 28, "C");
			text = "Custom";
			break;
		case 31:
			icon = _settings.timerOn ? img_menu_timer_on_png_comp : img_menu_timer_off_png_comp;
			text = _settings.timerOn ? "Timer is on" : "Timer is off";
			break;
		case 32:
			drawRoundRect(133,84,133,84,12,6);
			//icon = 1;
			text = "Set timer";
			break;
		case 51:
			
			//icon = img_menu_program_setup_icon_png_comp;
			//int16_t height;
			//if (pxs.sizeCompressedBitmap(width, height, img_prog_pattern_png_comp) == 0)
			//{
			//	pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_prog_pattern_png_comp);
			//}		
			//if (pxs.sizeCompressedBitmap(width, height, img_menu_program_setup_icon_png_comp) == 0)
			//{
			//	pxs.drawCompressedBitmap(320 / 2 +3, 240 / 2 +1, img_menu_program_setup_icon_png_comp);
			//}			
			
			text = "Setup";
			break;
		case 52:
			//icon = _settings.calendarOn ? img_program_cal_on_icon_png_comp : img_program_cal_off_icon_png_comp;

//			if (pxs.sizeCompressedBitmap(width, height, img_prog_pattern_png_comp) == 0)
		//	{
		//		pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_prog_pattern_png_comp);
		//	}		
		//	if (pxs.sizeCompressedBitmap(width, height, _settings.calendarOn ? img_program_cal_on_icon_png_comp : img_program_cal_off_icon_png_comp) == 0)
		//	{
		//		pxs.drawCompressedBitmap(320 / 2 +3, 240 / 2 +3, _settings.calendarOn ? img_program_cal_on_icon_png_comp : img_program_cal_off_icon_png_comp);
		//	}			
			text = _settings.calendarOn ? "On" : "Off";
			break;
		case 53:
			//icon = img_menu_program_custom_png_comp;
			text = "Custom day";
			break;
		case 41:
			//icon = img_menu_setdate_png_comp;
			text = "Date & time";
			break;
		case 42:
			drawRoundRect(92,84,92,84,12,6);
			icon = img_menu_display_png_comp;
			text = "Display";
			break;
		case 43:
			icon = _settings.soundOn ? img_menu_setting_sound_on_png_comp : img_menu_setting_sound_off_png_comp;
			text = "Sound";
			break;
		case 44:
			icon = img_menu_setting_service_png_comp;
			text = "Service";
			break;
		case 441:
			icon = img_menu_setting_reset_png_comp ;
			text = "Reset";
			break;
		case 442:
			pxs.setFont(FuturaBookC29a);
			DrawTextAligment(160 - (pxs.getTextWidth("Customer") / 2), 240/2 - 35, pxs.getTextWidth("Customer"), pxs.getTextLineHeight(), "Customer", false, false);
			DrawTextAligment(160 - (pxs.getTextWidth("service") / 2), 240/2 - 35 + pxs.getTextLineHeight(), pxs.getTextWidth("service"), pxs.getTextLineHeight(), "service", false, false);
			//drawRoundRect(92,84,92,84,12,6);
			//icon = img_menu_setting_info_png_comp ;
			//text = "Information";
			break;
		case 443:
			icon = img_menu_conn_png_comp;
			text = "WiFi reconnect";
			break;
		case 4431:
			drawRoundRect(133,84,133,84,12,6);
			pxs.setColor(MAIN_COLOR);
//			pxs.setFont(FuturaBookC20a);
			width = pxs.getTextWidth("START");
			pxs.print(320 / 2 - width / 2-1, 240/2 - 10, "START");		
			text = "Connecting";
			break;	
		case 4432:
			icon = img_menu_setting_reset_png_comp;
			text = "Reset connection";
			break;		
		case 411:
			icon = img_menu_program_icon_png_comp;
			text = "Set date";
			break;
		case 412:
			icon = img_menu_settime_png_comp;
			text = "Set time";
			break;
		case 421:
			icon = img_menu_display_bri_png_comp;
			text = "Brightness";
			break;
		case 422:
			drawRoundRect(92,84,92,84,12,6);
			icon = img_menu_display_auto_png_comp;
			text = "Auto switch off";
			break;
	}
	
	if (icon != NULL)
	{
		int16_t width, height;
		if (pxs.sizeCompressedBitmap(width, height, icon) == 0)
		{
			pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, icon);
		}
	}
	else
	{
		if(currentMenu->items[currentMenu->selected].ID == 32)
		{
			pxs.setFont(FuturaBookC36a);
			char buf[6];
			sprintf(buf, "%02d:%02d", (timer_time_set / 60), (timer_time_set % 60));
			DrawTextSelected(SW / 2 - pxs.getTextWidth(buf)/2, 103, buf, false, false, 0,0);
		}				
	}
	
	if (text != NULL)
		DrawMenuText(text);
}

void MainScreen()
{
	
	currentMenu = NULL;
	DrawMainScreen();
		
}

void refresh_mainscreen()
{
	if(currentMenu != NULL)
	{
		currentMenu = NULL;
		DrawMainScreen();
	}
}

void DrawCustomDay(int _old = -1)
{
	struct Presets* _pr = NULL;
//	pxs.setFont(FuturaBookC20a); // 18?
	
	_pr = &_settings.week_schedule[_presetSet.week_day];
	for (int iy = 0; iy < 4; iy++)
	{
		for (int ix = 0; ix < 6; ix++)
		{
			int i = iy * 6 + ix;
			
			int cX = ix * 48 + 19;
			int cY = iy * 38 + 65;

			char buf[4];
			sprintf(buf, "%d", _pr->hour[i]);
			
			if (i == _old || currentMenu->selected == i || _old == -1)
				DrawTextAligment(cX, cY, 42, 32, buf, false, false, (currentMenu->selected == i) ? 0 : (_pr->hour[i] > 10 ? 0: 2) , (currentMenu->selected == i) ? BG_COLOR : MAIN_COLOR, (currentMenu->selected == i) ? MAIN_COLOR : (_pr->hour[i] > 10 ? powerLevelColors[4] : BG_COLOR) , 0);
				//DrawTextAligment(cX, cY, 41, 31, buf, false, false, (currentMenu->selected == i) ? 2 : ((_pr->hour[i] == 3) ? 2 : 0), _pr->hour[i] == 3 ? MAIN_COLOR : BG_COLOR, 
				//																											currentMenu->selected == i ? RGB(255,237,0) : modeColors[_pr->hour[i]]);
		}
	}
}

void CustomNext()
{
	if (currentMenu->counts > 0)
	{
		int old = currentMenu->selected;
		currentMenu->selected++;
		if (currentMenu->selected >= currentMenu->counts)
			currentMenu->selected = 0;
		
		DrawCustomDay(old);
	}
}

void CustomPrev()
{
	if (currentMenu->counts > 0)
	{
		int old = currentMenu->selected;
		currentMenu->selected--;
		if (currentMenu->selected < 0)
			currentMenu->selected = currentMenu->counts - 1;
		
		DrawCustomDay(old);
	}
}

void MenuNext()
{

	if (currentMenu->next != NULL)
		currentMenu->next();
	else if (currentMenu->counts > 0)
	{
		smooth_backlight(0);	
		currentMenu->selected++;
		if (currentMenu->selected >= currentMenu->counts)
			currentMenu->selected = 0;
	
		DrawMenu();
		smooth_backlight(1);	
	}
}

void MenuPrev()
{
	#ifdef DEBUG
	printf("menu prev %d\n", currentMenu->ID);
	#endif
	

	if (currentMenu->prev != NULL)
		currentMenu->prev();
	else if (currentMenu->counts > 0)
	{
		smooth_backlight(0);	
		currentMenu->selected--;
		if (currentMenu->selected < 0)
			currentMenu->selected = currentMenu->counts - 1;
		
		DrawMenu();
		smooth_backlight(1);	
	}
}

void MenuOK()
{
	if (currentMenu->enter != NULL)
	{
		currentMenu->enter();
		return;
	}
	
	MenuItem_t* oldMenu = currentMenu;
	
	if (currentMenu->items != NULL && currentMenu->counts > 0)
	{
		MenuItem_t* nextMenu = &currentMenu->items[currentMenu->selected];
		
		nextMenu->parent = currentMenu;
		currentMenu = nextMenu;
		currentMenu->selected = 0;
		//if (currentMenu->ID == 2 &&_settings.heatMode == HeatMode_User)
			//currentMenu->selected = 1;
	}
	
	if((currentMenu->ID == 11 || currentMenu->ID == 12 || currentMenu->ID == 13))
	{
		pxs.setColor(BG_COLOR);
		pxs.fillRectangle(70, 70, 200, 200);		
	}

	if(currentMenu->ID == 421 || currentMenu->ID == 43)
	{
		smooth_backlight(0);
		pxs.clear();
	}
	
	if (currentMenu->ID == 5)
	{
		rtc_current_time_get(&rtc_initpara); 
		if ((bcdToDec(rtc_initpara.rtc_year) <= 19) && (bcdToDec(rtc_initpara.rtc_month) < 12))
		{
			old = currentMenu->parent;
			currentMenu = &_settingsMenu[0];
			currentMenu->parent = old;
		}
	}

	// first enter from tree menu to edit parameter
	if (oldMenu->items != NULL && currentMenu->items == NULL)
		PrepareEditParameter();
	else
		AcceptParameter();
	smooth_backlight(0);
	DrawMenu();
	smooth_backlight(1);
}

void GoOK(int step = 1)
{
	_timeoutSaveFlash = GetSystemTick();

	if ((currentMenu->ID == 411) && (currentMenu->parent->parent->selected == 4))
	{
		_modeOK.parent = currentMenu->parent;
		currentMenu = &_datetimeMenu[1];
		currentMenu->parent = _modeOK.parent;
		smooth_backlight(0);
		pxs.clear();
		pxs.setColor(MAIN_COLOR);
		pxs.fillRoundRectangle(320/2 - 92/2,240/2 - 84/2,92,84,12);
		pxs.setFont(FuturaBookC36a);
		pxs.setColor(BG_COLOR);
		pxs.setBackground(MAIN_COLOR);	
		int16_t width = pxs.getTextWidth("OK");
		int16_t height = pxs.getTextLineHeight();
		pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
		pxs.setColor(MAIN_COLOR);
		pxs.setBackground(BG_COLOR);
		smooth_backlight(1);
		delay_1ms(2000);
	}	
	else if ((currentMenu->ID == 412) && (currentMenu->parent->parent->selected == 4))
	{
		_modeOK.parent = currentMenu->parent;
		currentMenu = &_mainMenu[4];
		currentMenu->parent = old;
		smooth_backlight(0);
		pxs.clear();
		pxs.setColor(MAIN_COLOR);
		pxs.fillRoundRectangle(320/2 - 92/2,240/2 - 84/2,92,84,12);
		pxs.setFont(FuturaBookC36a);
		pxs.setColor(BG_COLOR);
		pxs.setBackground(MAIN_COLOR);	
		int16_t width = pxs.getTextWidth("OK");
		int16_t height = pxs.getTextLineHeight();
		pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
		pxs.setColor(MAIN_COLOR);
		pxs.setBackground(BG_COLOR);
		smooth_backlight(1);
		delay_1ms(2000);
	}
  else
	{
		_modeOK.parent = currentMenu->parent;
		if (step == 2)
			_modeOK.parent = _modeOK.parent->parent;
		currentMenu = &_modeOK;
	}
	idleTimeout = GetSystemTick();
}

void DrawDateEdit()
{
	char buf_day[3];
	char buf_month[3];
	char buf_year[5];
	sprintf(buf_day, "%02d", _dateTime.tm_mday);
	sprintf(buf_month, "%02d", _dateTime.tm_mon);
	sprintf(buf_year, "%04d", _dateTime.tm_year);
	
	pxs.setFont(FuturaBookC36a);
	int16_t y = 240 / 2 - pxs.getTextLineHeight();

	//DrawTextSelected(30, y, buf_year, (currentMenu->selected == 0), false, 5, 15);
	//DrawTextSelected(160, y, buf_month, (currentMenu->selected == 1), false, 5, 15);
	//DrawTextSelected(240, y, buf_day, (currentMenu->selected == 2), false, 5, 15);
	
	uint8_t widthX = pxs.getTextWidth(buf_year);
	DrawTextAligment(15, y, widthX + 20, 60, buf_year, (currentMenu->selected == 0),0,0,  /*(currentMenu->selected == 0) ? GREEN_COLOR :*/ MAIN_COLOR, /*(currentMenu->selected == 0) ? MAIN_COLOR   :*/ BG_COLOR );
	widthX = pxs.getTextWidth(buf_month);
	DrawTextAligment(150, y, widthX + 20, 60, buf_month, (currentMenu->selected == 1),0,0,  /*(currentMenu->selected == 1) ? GREEN_COLOR :*/ MAIN_COLOR, /*(currentMenu->selected == 1) ? MAIN_COLOR   :*/ BG_COLOR );
	pxs.getTextWidth(buf_day);
	DrawTextAligment(230, y, widthX + 20, 60, buf_day,   (currentMenu->selected == 2),0,0,  /*(currentMenu->selected == 2) ? GREEN_COLOR :*/ MAIN_COLOR,  /*(currentMenu->selected == 2) ? MAIN_COLOR :*/ BG_COLOR );
	
	
	

	pxs.setColor(MAIN_COLOR);
	pxs.setBackground(BG_COLOR);
	DrawMenuText((currentMenu->selected == 0) ? "Set year" : (currentMenu->selected == 1) ? "Set month" : "Set day");
}
void DrawTimeEdit()
{
	char buf[10];
	pxs.setFont(FuturaBookC36a);
	int16_t y = 240 / 2 - pxs.getTextLineHeight() ;

		sprintf(buf, "%02d", _dateTime.tm_hour);
		uint8_t widthX = pxs.getTextWidth(buf);
		DrawTextAligment(SW/2 - widthX/2 - 73, y, 70, 60, buf, (currentMenu->selected == 0),0,0,  /*(currentMenu->selected == 0) ? GREEN_COLOR : */MAIN_COLOR, /*(currentMenu->selected == 0) ? MAIN_COLOR : */BG_COLOR );
		sprintf(buf, "%02d", _dateTime.tm_min);
		widthX = pxs.getTextWidth(buf);
		DrawTextAligment(SW/2 - widthX/2 + 43, y, 70, 60, buf, (currentMenu->selected == 1),0,0,  /*(currentMenu->selected == 1) ? GREEN_COLOR : */MAIN_COLOR, /*(currentMenu->selected == 1) ? MAIN_COLOR : */BG_COLOR );	
		
		pxs.setColor(MAIN_COLOR);
		pxs.setBackground(BG_COLOR);
		DrawTextAligment(SW / 2 - pxs.getTextWidth(":") / 2, y, 4, 55, ":",0);
		DrawMenuText((currentMenu->selected == 0) ? "Set hour" : "Set minute");
	
}

void DrawEditParameter()
{
	char buf[30];
	int16_t width, height;
	struct Presets* _pr = NULL;
	if(!(currentMenu->ID == 11 || currentMenu->ID == 12 || currentMenu->ID == 13 || currentMenu->ID == 421 || currentMenu->ID == 43))
		pxs.clear();

	
	switch (currentMenu->ID)
	{
		case 999:
			pxs.setColor(MAIN_COLOR);
			pxs.fillRoundRectangle(320/2 - 92/2,240/2 - 84/2,92,84,12);
			pxs.setFont(FuturaBookC36a);
			pxs.setColor(BG_COLOR);
			pxs.setBackground(MAIN_COLOR);	
			int16_t width = pxs.getTextWidth("OK");
			int16_t height = pxs.getTextLineHeight();
		  pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
		  pxs.setColor(MAIN_COLOR);
			pxs.setBackground(BG_COLOR);
			break;
		case 12: // eco
			DrawLeftRight();
			DrawMenuTitle2("Eco < Comfort");
			DrawTemperature(_tempConfig.desired, -25, 15);
			break;
		case 11: // comform
		case 13: // anti
			DrawLeftRight();
			DrawTemperature(_tempConfig.desired, -25, 15);
			break;
		case 14: // anti
			DrawLeftRight();
			DrawTemperature(_tempConfig.desired, -25, 15);
			break;		
		case 2: // power custom
			DrawMenuText("Power level");

			width = (currentMenu->selected + 1) * 12 + currentMenu->selected * 20;
			for (int i = 0; i < currentMenu->selected + 1; i++)
			{
				pxs.setColor(powerLevelColors[i]);
				pxs.fillRectangle(320 / 2 - width / 2 + i * 12 + i * 20, 75, 12, 90);
			}
			pxs.setColor(MAIN_COLOR);
			DrawLeftRight();
			break;
		case 3:
			DrawLeftRight();
			currentMenu->selected ? DrawMenuText("Turn off") : DrawMenuText("Cancel Turn off");
			pxs.setFont(FuturaMediumC57a);
			char timer_buf[3];
			sprintf(timer_buf, "%dH", currentMenu->selected);
			DrawTextAligment(155 - (pxs.getTextWidth(timer_buf) / 2), 240/2 - 32, pxs.getTextWidth(timer_buf), pxs.getTextLineHeight(), timer_buf, false, false);			
			break;
		case 6: // power custom
			DrawMenuText("Change units");
			pxs.setFont(FuturaMediumC57a);
			if(currentMenu->selected == 1)
			{
				DrawTextAligment(145 - (pxs.getTextWidth("�F") / 2), 240/2 - 35, pxs.getTextWidth("�F"), pxs.getTextLineHeight(), "�F", false, false);	
			}
			else
			{
				DrawTextAligment(145 - (pxs.getTextWidth("�C") / 2), 240/2 - 35, pxs.getTextWidth("�C"), pxs.getTextLineHeight(), "�C", false, false);	
			}				
			
			DrawLeftRight();
			break;			
		case 52:
			DrawMenuTitle("Programme");
			pxs.setFont(FuturaBookC36a);
			//DrawTextAligment(20, 100, 100, 100,"ON", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(200, 100, 100, 100,"OFF", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );		
			DrawTextAligment( 20, 100, 100, 100, "ON",  _onoffSet.parameter,0,0,  MAIN_COLOR, BG_COLOR );
			DrawTextAligment(200, 100, 100, 100,"OFF", !_onoffSet.parameter,0,0,  MAIN_COLOR, BG_COLOR );				
			break;
		case 31:
		case 43:
		case 422:
			pxs.setFont(FuturaBookC36a);
			//DrawTextAligment( 20, 70, 100, 100,  "ON", _onoffSet.parameter,_onoffSet.current,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(200, 70, 100, 100, "OFF", !_onoffSet.parameter,!_onoffSet.current,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );		
			DrawTextAligment( 20, 70, 100, 100,  "ON",  _onoffSet.parameter, _onoffSet.current,0, MAIN_COLOR, BG_COLOR );
			DrawTextAligment(200, 70, 100, 100, "OFF", !_onoffSet.parameter,!_onoffSet.current,0, MAIN_COLOR, BG_COLOR );				
			break;
		case 421:
			pxs.setFont(FuturaBookC36a);
			//DrawTextAligment(20, 60, 120, 120,"50%", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(180, 60, 120, 120,"100%", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			DrawTextAligment( 20, 60, 120, 120, "50%",  _onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );
			DrawTextAligment(180, 60, 120, 120,"100%", !_onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );
			break;
		case 32:
			DrawTimeEdit();
			break;
		case 411: // set date
			DrawDateEdit();
			break;
		case 412: // set time
			DrawTimeEdit();
			break;
		case 441: // reset
			
			if (currentMenu->selected == 0)
			{
				pxs.setFont(FuturaBookC29a);
				DrawTextAligment(0, 0, 320, 60, "Reset all device", false);
				DrawTextAligment(0, 45, 320, 60, "settings", false);
				pxs.setFont(Arial25a);
				DrawTextAligment(220, 42, 10, 60, "?", false);
			}
			else if (currentMenu->selected == 1)
			{
				pxs.setFont(FuturaBookC29a);
				DrawTextAligment(0, 10, 320, 60, "Are you sure", false);
				pxs.setFont(Arial25a);
				DrawTextAligment(262, 8, 10, 60, "?", false);
			}
			
			pxs.setFont(FuturaBookC29a);
			//DrawTextAligment(20, 115, 90, 90,"Yes", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(210, 115, 90, 90,"No", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );	
			DrawTextAligment( 20, 115, 90, 90,"Yes",  _onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );
			DrawTextAligment(210, 115, 90, 90, "No", !_onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );			
			
			//DrawTextAligment(20, 115, 90, 90,"Yes", !_onoffSet.parameter,0,0,  _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			//DrawTextAligment(210, 115, 90, 90,"No", _onoffSet.parameter,0,0, _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );				
			break;
		case 442: // info
			pxs.setFont(FuturaBookC20a);
			DrawTextAligment(160 - (pxs.getTextWidth("Email:") / 2), 40 , pxs.getTextWidth("Email:"), pxs.getTextLineHeight(), "Email:", false, false);
			DrawTextAligment(160 - (pxs.getTextWidth("BalluCustomerService") / 2), 45 + pxs.getTextLineHeight(), pxs.getTextWidth("BalluCustomerService"), pxs.getTextLineHeight(), "BalluCustomerService", false, false);
			DrawTextAligment(160 - (pxs.getTextWidth("@gmail.com") / 2), 50 + pxs.getTextLineHeight()*2, pxs.getTextWidth("@gmail.com"), pxs.getTextLineHeight(), "@gmail.com", false, false);
			//DrawTextAligment(0, 70, 320, 60, "Email,", false);
		
			char buffer[20];
		  sprintf(buffer, "%s%s", "V.", VERSION);		
		
			width = pxs.getTextWidth(buffer);
			DrawTextAligment(160 - (pxs.getTextWidth(buffer) / 2), 165, pxs.getTextWidth(buffer), pxs.getTextLineHeight(), buffer, false);		
			break;
		case 443: // wifi
			pxs.setFont(FuturaBookC29a);

			DrawTextAligment(160 - (pxs.getTextWidth("Reset WiFi and") / 2),  10, pxs.getTextWidth("Reset WiFi and"), pxs.getTextLineHeight(), "Reset WiFi and", false);
			DrawTextAligment(160 - (pxs.getTextWidth("search again") / 2) - 10, pxs.getTextLineHeight() + 10, pxs.getTextWidth("search again"), pxs.getTextLineHeight(), "search again", false);
			pxs.setFont(Arial25a);
			DrawTextAligment(253, pxs.getTextLineHeight() + 28, pxs.getTextWidth("?"), pxs.getTextLineHeight(), "?", false);
			pxs.setFont(FuturaBookC29a);
			//DrawTextAligment(20, 115, 90, 90,"Yes", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(210, 115, 90, 90,"No", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );	
			DrawTextAligment( 20, 115, 90, 90,"Yes",  _onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );
			DrawTextAligment(210, 115, 90, 90, "No", !_onoffSet.parameter,0,0, MAIN_COLOR, BG_COLOR );					
			//pxs.setFont(FuturaBookC29a);
			//pxs.print(60, 90, "Connecting");
			//reset_wifi_state();
			break;		
		case 51:
		  rtc_current_time_get(&rtc_initpara);
			for (int i = 0; i < 7; i++)
			{
				if (_settings.calendar[i] > 7) _settings.calendar[i] = 0;
				pxs.setColor(MAIN_COLOR);
//				pxs.setFont(FuturaBookC20a);
				//DrawTextAligment(_calendarInfo[i].x, _calendarInfo[i].y, 50, 30, (char*)_calendarInfo[i].week, false, false, 0);
				//pxs.setFont(FuturaMediumC22a);
				DrawTextAligment(_calendarInfo[i].x, _calendarInfo[i].y + 30, 60, 60, (char*)_calendarInfo[i].week, (currentMenu->selected == i), i == (rtc_initpara.rtc_day_of_week -1) ? 1 : 0 , 2, 
					                 MAIN_COLOR, BG_COLOR );
			}
			break;
	/*	case 510:
			
		  DrawCustomDay();
		
			switch (_presetSet.week_day)
			{
				case 0: DrawMenuTitle("Monday"); 		break;
				case 1: DrawMenuTitle("Tuesday"); 	break;
				case 2: DrawMenuTitle("Wednesday"); break;
				case 3: DrawMenuTitle("Thursday"); 	break;
				case 4: DrawMenuTitle("Friday"); 		break;
				case 5: DrawMenuTitle("Saturday"); 	break;
				case 6: DrawMenuTitle("Sunday"); 		break;
			}
			
			pxs.setFont(FuturaMediumC22a);
			for (int iy = 0; iy < 2; iy++)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					int i = iy * 4 + ix;
					int16_t cX = ix * 75 + 22;
					int16_t cY = iy * 75 + 80;
					DrawTextAligment(cX, cY, 50, 50, (char*)_calendarPresetName[i], (currentMenu->selected == i), (_settings.calendar[_presetSet.week_day] == i), 2, 
													 MAIN_COLOR,  BG_COLOR);
				}
			}
			break;
		case 511:
			switch (_presetSet.preset)
			{
				case 0: DrawMenuTitle("PRESET 1", -5); 	break;
				case 1: DrawMenuTitle("PRESET 2", -5); 	break;
				case 2: DrawMenuTitle("PRESET 3", -5); 	break;
				case 3: DrawMenuTitle("PRESET 4", -5); 	break;
				case 4: DrawMenuTitle("PRESET 5", -5); 	break;
				case 5: DrawMenuTitle("PRESET 6", -5); 	break;
				case 6: DrawMenuTitle("PRESET 7", -5); 	break;
				case 7: DrawMenuTitle("CUSTOM", -5); 		break;
			}

			pxs.setFont(FuturaBookC20a); // 18?
			_pr = (_presetSet.preset < 7) ? (struct Presets*)&_presets[_presetSet.preset] : &_settings.custom;

			for (int iy = 0; iy < 4; iy++)
			{
				for (int ix = 0; ix < 6; ix++)
				{
					int i = iy * 6 + ix;
					
					int cX = ix * 48 + 19;
					int cY = iy * 38 + 65;

					sprintf(buf, "%d", i);
					if (_pr->hour[i] > 3) _pr->hour[i] = pEco;
					DrawTextAligment(cX, cY, 42, 32, buf, false, false, ((_pr->hour[i] == 3) ? 2 : 0), _pr->hour[i] == 3 ? MAIN_COLOR : BG_COLOR, modeColors[_pr->hour[i]]);

				}
			}
			break;*/
			/*
		case 53: // custom day
			DrawMenuTitle("CUSTOM 24h", -3);
			DrawCustomDay();
			break;
		case 530: // custom day select mode
			DrawLeftRight();

			switch (currentMenu->selected)
			{
				case pComfort:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_comfort_png_comp) == 0) pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_menu_mode_comfort_png_comp);
					DrawMenuText("Comfort");
					break;
				case pEco:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_eco_png_comp) == 0) pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_menu_mode_eco_png_comp);
					DrawMenuText("Eco");
					break;
				case pAntiFrost:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_anti_png_comp) == 0) pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_menu_mode_anti_png_comp);
					DrawMenuText("Anti-frost");
					break;
				default:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_program_off_png_comp) == 0) pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_menu_program_off_png_comp);
					DrawMenuText("Off");
					break;
			}
			break;*/
		default:
			DrawMenuText("Not implemented");
			break;
	}
}

void PrepareEditParameter()
{
	#ifdef DEBUG
	printf("PrepareEditParameter %d\n", currentMenu->ID);
	#endif
	
	//RTC_TimeTypeDef sTime;
	//RTC_DateTypeDef sDate;

	switch (currentMenu->ID)
	{
		case 3:
			currentMenu->parent = NULL;
			if((timer_time_set % 60) == 0)
			{
				current_time = timer_time_set / 60;
			}
			currentMenu->selected = current_time;
			break;
		case 6:
			currentMenu->parent = NULL;
			currentMenu->selected = _settings.units;
			break;
		case 11: // comform
			_tempConfig.desired = _settings.tempComfort;
			_tempConfig.min = MIN_TEMP_COMFORT;
			_tempConfig.max = MAX_TEMP_COMFORT;
			break;
		case 12: // eco
			_tempConfig.desired = _settings.tempEco;
			_tempConfig.min = 3;
			_tempConfig.max = 7;
			break;
		case 13: // anti
			_tempConfig.desired = _settings.tempAntifrost;
			_tempConfig.min = 3;
			_tempConfig.max = 7;
			break;
		case 14: // custom
			_tempConfig.desired = _settings.week_schedule[_presetSet.week_day].hour[currentMenu->selected];
			_tempConfig.min = 0;
			_tempConfig.max = 40;
			break;		
		case 21: // power auto
			AcceptParameter();
			return;
			break;
		case 2: // power custom
			currentMenu->selected = _settings.powerLevel - 1;
			break;
		case 31: // timer on/off
			_onoffSet.current = _settings.timerOn;
			_onoffSet.parameter = _settings.timerOn;
			break;
		case 32: // timer time
			_dateTime.tm_hour = _settings.timerTime / 60;
			_dateTime.tm_min = _settings.timerTime % 60;
			_dateTime.tm_sec = 0;
			currentMenu->selected = 0;
			break;
		case 43: // sound on/off
			_onoffSet.current = _settings.soundOn;
			_onoffSet.parameter = _settings.soundOn;
			break;
		case 411: // set date
			rtc_current_time_get(&rtc_initpara); 
			_dateTime.tm_mday = bcdToDec(rtc_initpara.rtc_date);
			_dateTime.tm_mon = bcdToDec(rtc_initpara.rtc_month);
			_dateTime.tm_year = (bcdToDec(rtc_initpara.rtc_year)) < 23 ? 2023 : (bcdToDec(rtc_initpara.rtc_year)) + 2000;
			currentMenu->selected = 0;
			break;
		case 412: // set time
			rtc_current_time_get(&rtc_initpara); 
			_dateTime.tm_hour = bcdToDec(rtc_initpara.rtc_hour);
			_dateTime.tm_min  = bcdToDec(rtc_initpara.rtc_minute);
			_dateTime.tm_sec  = bcdToDec(rtc_initpara.rtc_second);
			currentMenu->selected = 0;
			break;
		case 52: // calendar on/off
			_onoffSet.current = _settings.calendarOn;
			_onoffSet.parameter = _settings.calendarOn;
			break;
		case 51: // presets
			currentMenu->selected = 0;
			break;
		case 53: // custom day
			currentMenu->selected = 0;
			break;
		case 510: // presets
			currentMenu->selected = 0;
			break;
		case 421: // bight 50/100
			_onoffSet.current = !_settings.brightness;
			_onoffSet.parameter = !_settings.brightness;
			break;
		case 422: // auto on/off
			_onoffSet.current = _settings.displayAutoOff;
			_onoffSet.parameter = _settings.displayAutoOff;
			break;
		case 441: // reset
			currentMenu->selected = 0;
			currentMenu->parent = NULL;
			_onoffSet.current = _onoffSet.current;
			_onoffSet.parameter = _onoffSet.parameter;
			break;
		case 443: // reset wifi
			currentMenu->selected = 0;
			currentMenu->parent = NULL;
			_onoffSet.current = _onoffSet.current;
			_onoffSet.parameter = _onoffSet.parameter;
			break;		
	}
}




void AcceptParameter()
{
	switch (currentMenu->ID)
	{
		case 2: // power custom
			//_settings.heatMode = HeatMode_User;
			_settings.powerLevel = currentMenu->selected + 1;
			if(_settings.workMode == WorkMode_Turbo)
			{				
				_settings.workMode = _settings.tmp_workMode;
				timer_time_set = 0;
				InitTimer();
			}
			_timeoutSaveFlash = GetSystemTick();
			GoOK(2);
			break;		
		case 3:
			_settings.timerTime = currentMenu->selected;
			if(currentMenu->selected) 
			{
				_settings.timerOn = 1;
			}
			else
			{
				_settings.timerOn = 0;
			}
			
		  if(_settings.timerOn)
			{		
				if(_settings.workMode == WorkMode_Turbo)
				{				
					_settings.workMode = _settings.tmp_workMode;
				}
				timer_time_set = _settings.timerTime * 60;
				_eventTimer = 0;
				InitTimer();
				_timeoutSaveFlash = GetSystemTick();
		  }
			GoOK();
			break;		
		case 6:
			_settings.units = currentMenu->selected;
			GoOK();			
			break;
		case 11: // comform
			_settings.tempComfort = _tempConfig.desired;
			GoOK();
			break;
		case 12: // eco
			_settings.tempEco = _tempConfig.desired;
			GoOK();
			break;
		case 14: // anti
			_settings.week_schedule[_presetSet.week_day].hour[currentMenu->selected] = _tempConfig.desired;
			GoOK();
			break;
		case 13: // anti
			_settings.tempAntifrost = _tempConfig.desired;
			GoOK();
			break;		
		case 21: // power auto
			_settings.heatMode = HeatMode_Auto;
		  power_limit = 20;
			GoOK(2);
			break;

		case 31:
			_settings.timerOn = _onoffSet.parameter;
		  if(_settings.timerOn)
			{
				if((getCalendarMode() == WorkMode_Off))
				{
					_settings.workMode = WorkMode_Comfort;
				}
				_settings.calendarOn = 0;
				_eventTimer = 0;
				InitTimer();
		  }
			GoOK();
			break;
		case 32:
			currentMenu->selected++;
			if ((currentMenu->selected > 0) && (currentMenu->selected < 2))
			{		
				_settings.timerTime = _dateTime.tm_hour * 60 + _dateTime.tm_min;
				timer_time_set = _settings.timerTime;
				pxs.clear();
				pxs.setColor(MAIN_COLOR);
				pxs.fillOval(320/2 - 95/2,240/2 - 95/2, 95,95);
				pxs.setFont(FuturaBookC36a);
				pxs.setColor(BG_COLOR);
				pxs.setBackground(MAIN_COLOR);	
				int16_t width = pxs.getTextWidth("OK");
				int16_t height = pxs.getTextLineHeight();
				pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
				pxs.setColor(MAIN_COLOR);
				pxs.setBackground(BG_COLOR);
				delay_1ms(1000);
				pxs.clear();
				_timeoutSaveFlash = GetSystemTick();
				idleTimeout = GetSystemTick();	
				InitTimer();				
			}		
			if (currentMenu->selected == 2)
			{
				_settings.timerTime = _dateTime.tm_hour * 60 + _dateTime.tm_min;
				timer_time_set = _settings.timerTime;
				GoOK();
				InitTimer();
			}					
			break;
		case 43:
			_settings.soundOn = _onoffSet.parameter;
			GoOK();
			break;
		case 411: // date
			currentMenu->selected++;
		  rtc_current_time_get(&rtc_init_param); 
			if ((currentMenu->selected > 0) && (currentMenu->selected < 3))
			{
				if (isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				{
					_dateTime.tm_mon--;
					_dateTime.tm_year -= 1900;

					time_t time_temp = mktime(&_dateTime);
					const struct tm* time_out = localtime(&time_temp);
					
					//rtc_deinit();
					rtc_init_param.rtc_day_of_week = time_out->tm_wday == 0 ? RTC_SUNDAY : time_out->tm_wday;
					rtc_init_param.rtc_year = decToBcd(_dateTime.tm_year - 100);
					rtc_init_param.rtc_month = decToBcd(_dateTime.tm_mon + 1);
					rtc_init_param.rtc_date = decToBcd(_dateTime.tm_mday);
					
					//rtc_init_param.rtc_factor_asyn = 0x7FU;
					//rtc_init_param.rtc_factor_syn = 0xFFU;
					//rtc_initpara.rtc_display_format = RTC_24HOUR;					
					
					rtc_init(&rtc_init_param);
	
					_dateTime.tm_mon++;
					_dateTime.tm_year += 1900;
				}
				pxs.clear();
				pxs.setColor(MAIN_COLOR);
				pxs.fillRoundRectangle(320/2 - 92/2,240/2 - 84/2,92,84,12);
				pxs.setFont(FuturaBookC36a);
				pxs.setColor(BG_COLOR);
				pxs.setBackground(MAIN_COLOR);	
				int16_t width = pxs.getTextWidth("OK");
				int16_t height = pxs.getTextLineHeight();
				pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
				pxs.setColor(MAIN_COLOR);
				pxs.setBackground(BG_COLOR);
				delay_1ms(1000);
				pxs.clear();
				_timeoutSaveFlash = GetSystemTick();
				idleTimeout = GetSystemTick();
			}
					
			if (currentMenu->selected == 3)
			{
				if (isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				{
					_dateTime.tm_mon--;
					_dateTime.tm_year -= 1900;

					time_t time_temp = mktime(&_dateTime);
					const struct tm* time_out = localtime(&time_temp);
					
					//rtc_parameter_struct rtc_init_param;
					//rtc_deinit();
					rtc_init_param.rtc_day_of_week = time_out->tm_wday == 0 ? RTC_SUNDAY : time_out->tm_wday;
					rtc_init_param.rtc_year = decToBcd(_dateTime.tm_year - 100);
					rtc_init_param.rtc_month = decToBcd(_dateTime.tm_mon + 1);
					rtc_init_param.rtc_date = decToBcd(_dateTime.tm_mday);
					//rtc_init_param.rtc_factor_asyn = 0x7FU;
					//rtc_init_param.rtc_factor_syn = 0xFFU;
					//rtc_initpara.rtc_display_format = RTC_24HOUR;		
					rtc_init(&rtc_init_param);
	
					GoOK();
				}
				else
				{
					currentMenu->selected = 0;
				}
			}
			break;
		case 412: // time
			currentMenu->selected++;
			rtc_current_time_get(&rtc_init_param); 
			if ((currentMenu->selected > 0) && (currentMenu->selected < 2))
			{		
				
				rtc_init_param.rtc_hour = decToBcd(_dateTime.tm_hour);
				rtc_init_param.rtc_minute = decToBcd(_dateTime.tm_min);
				rtc_init_param.rtc_second = 0x00;
        //rtc_init_param.rtc_factor_asyn = 0x7FU;
        //rtc_init_param.rtc_factor_syn = 0xFFU;
				rtc_init(&rtc_init_param);
				pxs.clear();
				pxs.setColor(MAIN_COLOR);
				pxs.fillRoundRectangle(320/2 - 92/2,240/2 - 84/2,92,84,12);
				pxs.setFont(FuturaBookC36a);
				pxs.setColor(BG_COLOR);
				pxs.setBackground(MAIN_COLOR);	
				int16_t width = pxs.getTextWidth("OK");
				int16_t height = pxs.getTextLineHeight();
				pxs.print(320 / 2 - width/2-2, 240/2 - height/2, "OK");
				pxs.setColor(MAIN_COLOR);
				pxs.setBackground(BG_COLOR);
				delay_1ms(1000);
				pxs.clear();
				_timeoutSaveFlash = GetSystemTick();
				idleTimeout = GetSystemTick();				
			}
			if (currentMenu->selected == 2)
			{
				
				rtc_init_param.rtc_hour = decToBcd(_dateTime.tm_hour);
				rtc_init_param.rtc_minute = decToBcd(_dateTime.tm_min);
				rtc_init_param.rtc_second = 0x00;
        //rtc_init_param.rtc_factor_asyn = 0x7FU;
        //rtc_init_param.rtc_factor_syn = 0xFFU;
				rtc_init(&rtc_init_param);
				GoOK();				
			}
			break;
		case 52: // calendar
			_settings.calendarOn = _onoffSet.parameter;		
		  if(_settings.calendarOn)
			{
				_settings.heatMode = HeatMode_Auto;
				_settings.timerOn = 0;
				_eventTimer = 0;
				InitTimer();
		  }		
			else
			{
				_settings.workMode = WorkMode_Comfort;
			}
			GoOK();	
			break;
			/*
		case 53: // custom day
			uint8_t select = currentMenu->selected;
			_selectModeMenu.parent = currentMenu;
			currentMenu = &_selectModeMenu;
			currentMenu->selected = _settings.custom.hour[select];
			break;
		case 530: // custom day
			_settings.custom.hour[currentMenu->parent->selected] = currentMenu->selected;
			GoOK();
			break;
		case 51: // presets
			_presetSet.week_day = currentMenu->selected;
			//_presetSet.preset = _settings.calendar[_presetSet.week_day];
			_presetMenu.parent = currentMenu;
			currentMenu = &_presetMenu;
			currentMenu->selected = 0;
			break;
		case 510: // presets
			//_presetSet.preset = currentMenu->selected;
			//_presetViewMenu.parent = currentMenu;
			//currentMenu = &_tempshedMenu;
			//currentMenu->selected = 0;
			//break;
		case 511: // presets
			_settings.calendar[_presetSet.week_day] = _presetSet.preset;
			GoOK();
			break;
		case 421:
			_settings.brightness = !_onoffSet.parameter;
			if (_settings.brightness)
				LL_GPIO_SetOutputPin(LCD_BL_GPIO_Port, LCD_BL_Pin);
			GoOK();*/
		case 421:
			_settings.brightness = !_onoffSet.parameter;
			if (_settings.brightness)
			{
				//LL_GPIO_SetOutputPin(LCD_BL_GPIO_Port, LCD_BL_Pin);
				_stateBrightness = StateBrightness_ON;
			}
			else
			{
				_stateBrightness = StateBrightness_LOW;
			}
			GoOK();					
			break;
		case 422:
			_settings.displayAutoOff = _onoffSet.parameter;
			GoOK();
			
			break;
		case 441: // reset
			if (!_onoffSet.parameter)
			{
				MenuBack();
				break;
			}
			
			currentMenu->selected++;
			if (currentMenu->selected == 1)
				_onoffSet.current = _onoffSet.parameter = 1;
			else if (currentMenu->selected == 2)
			{
				ResetAllSettings();
				SaveFlash();
				GoOK();
				//NVIC_SystemReset();
			}
			break;
			
		case 443: // wifi
			if (!_onoffSet.parameter)
			{
				MenuBack();
				break;
			}		
		
			reset_wifi_state();
			GoOK();
			break;		
		case 4432: // wifi
		//reset_wifi_state();
		GoOK();
		break;	
	}
}

void TempMinus()
{
	CleanTemperature(_tempConfig.desired,-25, 15);
	_tempConfig.desired--;
	if (_tempConfig.desired < _tempConfig.min)
			_tempConfig.desired = _tempConfig.max;

	DrawEditParameter();
}

void TempPlus()
{
	CleanTemperature(_tempConfig.desired,-25, 15);
	_tempConfig.desired++;
	if (_tempConfig.desired > _tempConfig.max)
			_tempConfig.desired = _tempConfig.min;

	DrawEditParameter();
}

void DateMinus()
{
		#ifdef DEBUG
	printf("DateMinus\n");
	#endif
	
	
	if (currentMenu->selected == 2)
	{
		_dateTime.tm_mday--;
		if (_dateTime.tm_mday == 0)
		{
			_dateTime.tm_mday = 31;
			while (!isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				_dateTime.tm_mday--;
		}
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_mon--;
		if (_dateTime.tm_mon == 0)
			_dateTime.tm_mon = 12;
	}
	else if (currentMenu->selected == 0)
	{
		_dateTime.tm_year--;
		if (_dateTime.tm_year < 2019)
			_dateTime.tm_year = 2099;
	}
	DrawEditParameter();
}

void DatePlus()
{
		#ifdef DEBUG
	printf("DatePlus\n");
	#endif
	
	
	if (currentMenu->selected == 2)
	{
		_dateTime.tm_mday++;
		if (!isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
			_dateTime.tm_mday = 1;
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_mon++;
		if (_dateTime.tm_mon == 13)
			_dateTime.tm_mon = 1;
	}
	else if (currentMenu->selected == 0)
	{
		_dateTime.tm_year++;
		if (_dateTime.tm_year == 2100)
			_dateTime.tm_year = 2019;
	}
	DrawEditParameter();
}

void TimeMinus()
{
	if (currentMenu->selected == 0)
	{
		_dateTime.tm_hour--;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0) && (_dateTime.tm_min == 0))
		{
			_dateTime.tm_min = 1;
		}
		if (_dateTime.tm_hour < 0)
			_dateTime.tm_hour = 23;
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_min--;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0))
		{			
			if (_dateTime.tm_min < 1)
			_dateTime.tm_min = 59;
		}
		else
		{
		if (_dateTime.tm_min < 0)
			_dateTime.tm_min = 59;
		}
	}
	DrawEditParameter();
}

void TimePlus()
{
	if (currentMenu->selected == 0)
	{
		_dateTime.tm_hour++;
		if (_dateTime.tm_hour > 23)
			_dateTime.tm_hour = 0;

		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0) && (_dateTime.tm_min == 0))
		{
			_dateTime.tm_min = 1;
		}
	}
	else if (currentMenu->selected == 1)
	{		
		_dateTime.tm_min++;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0))
		{			
			if (_dateTime.tm_min > 59)
			_dateTime.tm_min = 1;
		}
		else
		{
		if (_dateTime.tm_min > 59)
			_dateTime.tm_min = 0;
		}
	}
	DrawEditParameter();
}

void Off()
{
	_onoffSet.parameter = 0;
	DrawEditParameter();
}

void On()
{
	_onoffSet.parameter = 1;
	DrawEditParameter();
}
void MenuBack()
{

	if (currentMenu->parent != NULL)
	{
		// if back in schedule to calendar on/off
		smooth_backlight(0);
		if ((currentMenu->ID == 51) && (!_settings.calendarOn))
		{
			struct MenuItem* old = currentMenu->parent;
			currentMenu = &_programMenu[1];
			currentMenu->parent = old;
			_onoffSet.current = _settings.calendarOn;
			_onoffSet.parameter = _settings.calendarOn;
			
			DrawMenu();
		}
		else
		{
			if(currentMenu->ID == 441) 
			{
				_onoffSet.current = 0;
				_onoffSet.parameter = 0;				
			}
			currentMenu = currentMenu->parent;
			DrawMenu();
		}
		smooth_backlight(1);
	}
	else
	{
		MainScreen();
	}
}

const int MAX_VALID_YR = 2099; 
const int MIN_VALID_YR = 2023; 
  
bool isLeap(int year) 
{ 
	return (((year % 4 == 0) &&  (year % 100 != 0)) ||  (year % 400 == 0)); 
} 

bool isValidDate(int d, int m, int y) 
{ 
    // If year, month and day  
    // are not in given range 
    if (y > MAX_VALID_YR || y < MIN_VALID_YR) 
			return false; 
		if (m < 1 || m > 12) 
			return false; 
    if (d < 1 || d > 31) 
			return false; 
  
    // Handle February month  
    // with leap year 
    if (m == 2) 
    { 
        if (isLeap(y)) 
        return (d <= 29); 
        else
        return (d <= 28); 
    } 
  
    // Months of April, June,  
    // Sept and Nov must have  
    // number of days less than 
    // or equal to 30. 
    if (m == 4 || m == 6 || 
        m == 9 || m == 11) 
        return (d <= 30); 
  
    return true; 
} 


void EnterMenu()
{
	smooth_backlight(0);	
	currentMenu = &_menu;
	currentMenu->selected = 0;
	
	DrawMenu();
	smooth_backlight(1);	
}

void SetPower(int8_t value)
{
	if (value < 0)
		value = 0;
	if (value > 20)
		value = 20;
	
	_currentPower = value;
	
	
	if(_currentPower == 20)
	{
		if(_settings.powerLevel == 2)
		{
			LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_5);
		}
		else
		{
			LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_5);
		}
		LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_6);
	}
	else
	{
		LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_5);
		LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_6);
	}
	
	/*
	if(_settings.half_power)
	{
		if (gpio_input_bit_get(GPIOB, GPIO_PIN_5))
		{
			LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_5);
			delay_1ms(100);
		}
		semistor_power = value;
	}
	else
	{
		if (value > 10)
		{
			LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_5);
			delay_1ms(100);
			semistor_power = value*2 - 20;
		}
		else
		{
			if (gpio_input_bit_get(GPIOB, GPIO_PIN_5))
			{
				LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_5);
				delay_1ms(100);
			}
			semistor_power = value*2;
		}		
	}
	if(semistor_power == 20)
	{
		LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_6);
		delay_1ms(100);
	}
	else
	{
		if (gpio_input_bit_get(GPIOB, GPIO_PIN_6))
		{
			LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_6);
			delay_1ms(100);
		}
	}		
	delay_1ms(100);*/
}
	
void TIMER_Heat_callback()
{
		_currentPowerTicks--;

		if (semistor_power > _currentPowerTicks)
		{
			LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_8);
		}
		else
		{
			LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_8);
		}

		if (_currentPowerTicks == 0)
			_currentPowerTicks = 20;
}






void rtc_alarm_callback()
{

	if (_settings.timerOn ||  _settings.workMode == WorkMode_Turbo)
	{
		if (timer_time_set > 0)
		{
			timer_time_set--;
		}
		
		if (timer_time_set == 0)
		{
			timer_time_set = ( (_settings.workMode == WorkMode_Turbo) ? TIMER_TURBO : _settings.timerTime);
			_eventTimer = 1;
		}
		//query_settings();
	}
	else
	{
		_eventTimer = 1;
	}
}



void SysTick_Handler_Callback()
{
	
	
	
	//xw09A_read_data(1);
/*
	if (keyTimer-- <= 0)
	{
		_key_window.update();
		_key_power.update();
		_key_menu.update();
		_key_back.update();
		_key_down.update();
		_key_up.update();

		keyTimer = 5;
	}*/
}


void I2C_ClearBusyFlagErratum()
{

	 
  i2c_disable(I2C0);
	
	  gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);	
		PIN_SET(GPIOA, GPIO_PIN_9);
		gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);	
		PIN_SET(GPIOA, GPIO_PIN_10);

  // 3. Check SCL and SDA High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 4. Configure the SDA I/O as General Purpose Output Open-Drain, Low level (Write 0 to GPIOx_ODR).
  PIN_RESET(GPIOA, GPIO_PIN_9);

  //  5. Check SDA Low level in GPIOx_IDR.
  while (RESET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 6. Configure the SCL I/O as General Purpose Output Open-Drain, Low level (Write 0 to GPIOx_ODR).
  PIN_RESET(GPIOA, GPIO_PIN_10);

  //  7. Check SCL Low level in GPIOx_IDR.
  while (RESET != gpio_output_bit_get(GPIOA, GPIO_PIN_10))
  {
    __asm("nop");
  }

  // 8. Configure the SCL I/O as General Purpose Output Open-Drain, High level (Write 1 to GPIOx_ODR).
  PIN_SET(GPIOA, GPIO_PIN_10);

  // 9. Check SCL High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_10))
  {
    __asm("nop");
  }

  // 10. Configure the SDA I/O as General Purpose Output Open-Drain , High level (Write 1 to GPIOx_ODR).
  PIN_SET(GPIOA, GPIO_PIN_9);

  // 11. Check SDA High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 12. Configure the SCL and SDA I/Os as Alternate function Open-Drain.
    gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_9);
    gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,GPIO_PIN_9);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,GPIO_PIN_10);

  // 13. Set SWRST bit in I2Cx_CR1 register.
	i2c_software_reset_config(I2C0, I2C_SRESET_SET);

  __asm("nop");

  // 14. Clear SWRST bit in I2Cx_CR1 register.
	i2c_software_reset_config(I2C0, I2C_SRESET_RESET);

  __asm("nop");

  // 15. Enable the I2C peripheral by setting the PE bit in I2Cx_CR1 register
	i2c_enable(I2C0);

  // Call initialization function.
    i2c_clock_config(I2C0,400000,I2C_DTCY_16_9);
    /* configure I2C address */
    i2c_mode_addr_config(I2C0,I2C_I2CMODE_ENABLE,I2C_ADDFORMAT_7BITS,0x81);
    /* enable I2C0 */
    i2c_enable(I2C0);
}


uint16_t result;
uint8_t p_buffer[2];
uint8_t xw09A_read_data(uint8_t button_num)
{  

		uint32_t cnt = 0;
		
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}
		i2c_ackpos_config(I2C0,I2C_ACKPOS_NEXT);
		i2c_ack_config(I2C0, I2C_ACK_ENABLE);		
    i2c_start_on_bus(I2C0);
	
		cnt = 0;
		
		
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}			
    i2c_master_addressing(I2C0, 0x81, I2C_RECEIVER);
		
		while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}				
		i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
		i2c_stop_on_bus(I2C0);

		cnt = 0;

		while(!i2c_flag_get(I2C0, I2C_FLAG_RBNE))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}		
		p_buffer[0] = i2c_data_receive(I2C0);
		i2c_ack_config(I2C0, I2C_ACK_DISABLE);
	cnt = 0;
		while(!i2c_flag_get(I2C0, I2C_FLAG_RBNE))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}		
		p_buffer[1] = i2c_data_receive(I2C0);		
		i2c_stop_on_bus(I2C0);
        	
		//efff  f7ff
		//fbff	fdff
		//ff7f	ffbf

		result = ~(p_buffer[0] << 8 | p_buffer[1]);
    
		if(result & (1 << button_num))
			return true;
		else
			return false;
}

void beep()
{
	if (_settings.soundOn)
	{
		//timer_primary_output_config(TIMER16,ENABLE);
		//timer_enable(TIMER16);
		timer_channel_output_pulse_value_config(TIMER16,TIMER_CH_0,165);		
		delay_1ms(20);
		timer_channel_output_pulse_value_config(TIMER16,TIMER_CH_0,0);
		//timer_primary_output_config(TIMER16,DISABLE);
		//timer_disable(TIMER16);
	}
}






int16_t getTemperature()
{
//	HAL_ADC_Start(&hadc);
//	HAL_ADC_PollForConversion(&hadc, 100);
//	uint32_t raw = HAL_ADC_GetValue(&hadc);
//	HAL_ADC_Stop(&hadc);
	
	while(adc_flag_get(ADC_FLAG_EOC))
	{
		raw = adc_regular_data_read();
		delay_1ms(1);
		adc_flag_clear(ADC_FLAG_EOC);
	}
	
	if (raw >= 50 && raw <= 4000)
	{
		float R = BALANCE_RESISTOR * ((4095.0 / raw) - 1);
		//double tKelvin = (BETA * ROOM_TEMP) / (BETA + (ROOM_TEMP * std::log(float(R / RESISTOR_ROOM_TEMP))));
		//double tKelvin = (BETA + (ROOM_TEMP * std::log(float(R / RESISTOR_ROOM_TEMP)))) / (std::log(float(R / RESISTOR_ROOM_TEMP)));
		//return (int)(tKelvin - 273.15);
	  float steinhart;
		steinhart = R / RESISTOR_ROOM_TEMP; // (R/Ro)
		steinhart = std::log(float(steinhart)); // ln(R/Ro)
		steinhart /= BETA; // 1/B * ln(R/Ro)
		steinhart += 1.0 / (ROOM_TEMP); // + (1/To)
		steinhart = 1.0 / steinhart; // convert to Kelvin
		if(_settings.units)
		{
			steinhart = 9*(steinhart - 273.15)/5 + 32 - 11;	// convert to F with correction of 5.4F
		}
		else
		{
			steinhart -= 279.15; // convert to C with correction of 3C
		}
		
		if((steinhart <= (temp_steinhart + 3)) && (steinhart >= (temp_steinhart - 3))) // if temp increase or decrease impulsively, we skip averaging
		{
			steinhart = (steinhart * 0.3) + (temp_steinhart * 0.7);
		}
		temp_steinhart = steinhart;
		
		return (int16_t)rint(steinhart);
	}
	else if(raw < 50)
	{
		return -127;
	}
		else if(raw > 4000)
	{
		return 127;
	}
	return 0;
}

void DrawWindowOpen()
{
	_stateBrightness = StateBrightness_ON;
	if (_timerBlink < GetSystemTick())
	{
		_blink = !_blink;
		_timerBlink = GetSystemTick() + 1000;
		smooth_backlight(0);
		pxs.clear();
		if (_blink)
		{
			pxs.drawCompressedBitmap(100, 64, (uint8_t*)img_WindowOpen_png_comp);
			smooth_backlight(1);
		}
	}
}


void DrawWifi()
{
	if (wifi_status == -1)
	{
		if (_blink)
		{
			_blink = false;
			DrawMainScreen();
		}
		return;
	}
	
	if (_timerBlink < GetSystemTick())
	{
		_timerBlink = GetSystemTick();
		/*if (wifi_status == 2)
		{
			pxs.setColor(BG_COLOR);
			if (_settings.workMode == WorkMode_Off)
			{				
				pxs.fillRectangle(22, 59, 43, 26);				
			}
			else
			{
				pxs.fillRectangle(_xWifi + 11, 137, 83, 26);
			}
			pxs.setColor(MAIN_COLOR);
			return;
		}
	  else */
		if (wifi_status == 0){
			//_timerBlink += 250;
		  _timerBlink += 1000;}
		else if (wifi_status == 2)
			_timerBlink += 250;		
		else if (wifi_status == 3)
			_timerBlink += 500;
		else if (wifi_status == 4)
		{
			if (_settings.workMode == WorkMode_Off)
			{
				pxs.drawCompressedBitmap(22, 59, (uint8_t*)img_wifi_png_comp);
			}
			else
			{
				pxs.drawCompressedBitmap(_xWifi + 12, 137, (uint8_t*)img_wifi_png_comp);
			}
      //_timerBlink += 5000;			
			return;		
		}
		//if (wifi_status != 4)
		_blink = !_blink;
	
		if (_settings.workMode == WorkMode_Off)
		{
			pxs.setColor(BG_COLOR);
			pxs.fillRectangle(22, 59, 43, 26);
			if (_blink)
				pxs.drawCompressedBitmap(22, 59, (uint8_t*)img_wifi_png_comp);
			pxs.setColor(MAIN_COLOR);
		}
		else
		{
			pxs.setColor(BG_COLOR);
			pxs.fillRectangle(_xWifi + 12, 137, 83, 26);
			if (_blink)
				pxs.drawCompressedBitmap(_xWifi + 12, 137, (uint8_t*)img_wifi_png_comp);
			pxs.setColor(MAIN_COLOR);
		}
	}
}

void DrawTemperature(int16_t temp, int8_t xo, int8_t yo)
{
	pxs.setColor(MAIN_COLOR);
	char buffer[20];
	sprintf(buffer, "%d", temp);
	pxs.setFont(FuturaDemiC80a);
	int widthX = pxs.getTextWidth(buffer);
	int cX = 160 - widthX / 2 + xo;
	pxs.print(cX, 70 + yo, buffer);
	pxs.setFont(FuturaDemiC33a);
	int8_t kerning[] = {-5,-100};
	pxs.print(cX + widthX, 52 + yo, (_settings.units ? "�F" : "�C"), kerning);
	_xWifi = cX + widthX;
}

void CleanTemperature(int16_t temp, int8_t xo, int8_t yo)
{
	pxs.setColor(MAIN_COLOR);
	char buffer[20];
	sprintf(buffer, "%d", temp);
	pxs.setFont(FuturaDemiC80a);
	int widthX = pxs.getTextWidth(buffer);
	int cX = 160 - widthX / 2 + xo;
	pxs.cleanText(cX, 70 + yo, buffer);
	pxs.setFont(FuturaDemiC33a);
	int8_t kerning[] = {-5,-100};
	pxs.cleanText(cX + widthX, 52 + yo, (_settings.units ? "�F" : "�C"), kerning);
	_xWifi = cX + widthX;
}


uint8_t DrawError()
{
	char *err;
	_error = 0;
	if (temp_current  == -127)
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 1;
		err = "E1";
	}
	else if (temp_current  == 127)
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 2;
		err = "E2";
	}
	else if (temp_current > (_settings.units ? 118 : 48))
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 3;
		err = "E3";
	}
	else if (temp_current < (_settings.units ? -15 : -26))
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 4;
		err = "E4";
	}
	
	if(_settings.brightness) _stateBrightness = StateBrightness_ON;
	else _stateBrightness = StateBrightness_LOW;
	
	if(!_error_fl && _error)
	{
		_error_fl = 1;
		pxs.clear();
		pxs.setFont(FuturaDemiC80a);
		DrawTextAligment(165 - (pxs.getTextWidth(err) / 2), 90 - pxs.getTextLineHeight()/2, pxs.getTextWidth(err), pxs.getTextLineHeight(), err, false, false);
		pxs.setFont(FuturaBookC20a);
		DrawTextAligment(160 - (pxs.getTextWidth("Contact US:") /2), 						225 - pxs.getTextLineHeight() * 3, 	pxs.getTextWidth("Contact US:"), 						pxs.getTextLineHeight(), "Contact US:", 						false, false);
		DrawTextAligment(160 - (pxs.getTextWidth("BalluCustomerService") /2), 	225 - pxs.getTextLineHeight() * 2, 	pxs.getTextWidth("BalluCustomerService"), 	pxs.getTextLineHeight(), "BalluCustomerService", 		false, false);
		DrawTextAligment(160 - (pxs.getTextWidth("@gmail.com") /2), 						225 - pxs.getTextLineHeight() , 		pxs.getTextWidth("@gmail.com"), 						pxs.getTextLineHeight(), "@gmail.com", 							false, false);

		smooth_backlight(1);
		query_faults();
	}
	return _error;
}
	
void DrawMenuText(const char *text)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setFont(FuturaBookC29a);
	int16_t width = pxs.getTextWidth((char*)text);
	pxs.print(320 / 2 - width / 2, 185, (char*)text);
}

void DrawMenuTitle(const char *text, int8_t yo)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setFont(FuturaBookC29a);
	int16_t width = pxs.getTextWidth((char*)text);
	pxs.print(320 / 2 - width / 2, 26 + yo, (char*)text);
}

void DrawMenuTitle2(const char *text)
{
	pxs.setColor(MAIN_COLOR);
//	pxs.setFont(FuturaBookC20a);
	int16_t width = pxs.getTextWidth((char*)text);
	pxs.print(320 / 2 - width / 2, 16, (char*)text);
}

void DrawTextAligment(int16_t x, int16_t y, int16_t w, int16_t h, char* text, bool selected, bool underline, uint8_t border, RGB fore, RGB back, bool round)
{
	
	int16_t width = pxs.getTextWidth(text);
	int16_t height = pxs.getTextLineHeight();
	
	int16_t cX = x + w / 2 - width / 2;
	int16_t cY = y + h / 2 - height / 2;

	pxs.setColor(selected ? fore : back);
	if(round)
	{
		pxs.fillRoundRectangle(x, y, w, h, 7);
	}
	else
	{
		pxs.fillRectangle(x, y, w, h);
	}
	pxs.setColor(!selected ? fore : back);
	
	if(round)
	{
		if (border > 0 && !selected)
		{
			pxs.setColor(!selected ? fore : back);
			pxs.setBackground(selected ? fore : back);
			pxs.fillRoundRectangle(x, y, w, h, 7);
			pxs.setColor(selected ? fore : back);
			pxs.setBackground(!selected ? fore : back);
			pxs.fillRoundRectangle(x + border, y + border, w - border * 2, h - border * 2, 7);
			pxs.setColor(!selected ? fore : back);
			pxs.setBackground(selected ? fore : back);
		}
	}
	else
	{
		if (border > 0 && !selected)
		{
			for (int i = 0; i < border; i++)
			{
				pxs.drawRectangle(x + i, y + i, w - i * 2, h - i * 2);
			}
		}
	}
	
	
	
	pxs.setBackground(selected ? fore : back);
	pxs.print(cX, cY, text);

	if (underline)
	{
		pxs.setColor(!selected ? fore : back);
		pxs.fillRectangle(cX, cY + height + 5, width, 3);
	}

	pxs.setBackground(BG_COLOR);
	

}
	
void DrawTextSelected(int16_t x, int16_t y, char* text, bool selected, bool underline = false, int16_t oX = 5, int16_t oY = 5)
{
	pxs.setColor(selected ? MAIN_COLOR : BG_COLOR);
	int16_t width = pxs.getTextWidth(text);
	int16_t height = pxs.getTextLineHeight();
	pxs.fillRectangle(x - oX, y - oY, width + oX * 2, height + oY * 2);
	pxs.setColor(!selected ? MAIN_COLOR : BG_COLOR);
	pxs.setBackground(selected ? MAIN_COLOR : BG_COLOR);
	pxs.print(x, y, text);
	
	if (underline)
	{
		pxs.setColor(!selected ? MAIN_COLOR : BG_COLOR);
		pxs.fillRectangle(x, y + height + 5, width, 4);
	}

	pxs.setBackground(BG_COLOR);
}

void DrawMainScreen(uint32_t updater)
{
	
	if ((currentMenu != NULL) && (!_settings.blocked))
		return;
	
	if(_settings.lowScreenMode)
	{
		smooth_backlight(0);
		pxs.clear();
		pxs.setColor(RED_COLOR);
		pxs.fillOval(278, 16, 28, 28);
		pxs.setColor(MAIN_COLOR); 
		pxs.setBackground(BG_COLOR);				
		_stateBrightness = StateBrightness_LOW;
		smooth_backlight(1);	
		return;			
	}

	if (updater == 0x01)
	{
		//pxs.setColor(BG_COLOR);
		//pxs.fillRectangle(100, 60, 200, 95);
		//pxs.fillRectangle(200, 70, 80, 35);
	}
	else                                                                                            
	{
		smooth_backlight(0);
		pxs.clear();
	}
	
	
	pxs.setColor(BG_COLOR);
	//pxs.fillRectangle(_xWifi + 12, 137, 83, 26);


	pxs.setColor(MAIN_COLOR);
  if(_settings.calendarOn == 0)
	{
		switch (_settings.workMode)
		{
			case WorkMode_Comfort:
				DrawTemperature(getModeTemperature(),0,15);
				pxs.drawCompressedBitmap(12, 15, (uint8_t*)img_icon_comfort_png_comp);
				break;
			case WorkMode_Eco:
				DrawTemperature(getModeTemperature(),0,15);
				pxs.drawCompressedBitmap(15, 15, (uint8_t*)img_icon_eco_png_comp);
				break;
			case WorkMode_Turbo:
				pxs.drawCompressedBitmap(12, 13, (uint8_t*)img_icon_turbo_png_comp);
			
			  pxs.setFont(FuturaDemiC33a);
				DrawTextAligment(160 - (pxs.getTextWidth("Boost") /2), 									60, 																															pxs.getTextWidth("Boost"), 									pxs.getTextLineHeight(), "Boost", 									false, false);
				pxs.setFont(FuturaBookC20a);
				DrawTextAligment(160 - (pxs.getTextWidth("Heating at max power") /2), 	120 - pxs.getTextLineHeight() / 2, 																pxs.getTextWidth("Heating at max power"), 	pxs.getTextLineHeight(), "Heating at max power", 		false, false);
				DrawTextAligment(160 - (pxs.getTextWidth("for 3 hours") /2), 						120 - (pxs.getTextLineHeight() / 2) + pxs.getTextLineHeight()+3, 	pxs.getTextWidth("for 3 hours"), 						pxs.getTextLineHeight(), "for 3 hours", 						false, false);		
				DrawTextAligment(160 - (pxs.getTextWidth("<Press mode to cancel>") /2), 240 - pxs.getTextLineHeight()*2, 																	pxs.getTextWidth("<Press mode to cancel>"), pxs.getTextLineHeight(), "<Press mode to cancel>", 	false, false);
				break;
			case WorkMode_Off:
				pxs.drawCompressedBitmap(150, 60, (uint8_t*)img_menu_program_off_png_comp);
				pxs.setFont(FuturaBookC29a);
				DrawTextAligment(145, 160, 82, 45, "Off", false, false);
				break;
		}
	}
	else
	{
		//DrawTemperature(15, getCalendartemp(),0,15);
	}
	
	
	if(_settings.blocked && (_settings.workMode != WorkMode_Turbo)) 
		pxs.drawCompressedBitmap(18, 136, (uint8_t*)img_locked_png_comp);
/*
	if ((wifi_status == 4) && (currentMenu == NULL))
	{
		if(_settings.workMode == WorkMode_Off)
			pxs.drawCompressedBitmap(12, 50, (uint8_t*)img_wifi_png_comp);
		else
			pxs.drawCompressedBitmap(_xWifi + 12, 137, (uint8_t*)img_wifi_png_comp);	
	}
	else
	{
		pxs.setColor(BG_COLOR);
		if(_settings.workMode == WorkMode_Off)
			pxs.fillRectangle(12, 50, 43, 26);
		else
			pxs.fillRectangle(_xWifi + 12, 136, 83, 26);
		pxs.setColor(MAIN_COLOR);			
	}	
*/
		
		
	if (_settings.timerOn == 1 )
				pxs.drawCompressedBitmap(15, 77, (uint8_t*)img_icon_timer_png_comp);
	

	if (_settings.workMode != WorkMode_Off)
	{
		uint8_t powerLevel = 2;
		/*
		if (_settings.heatMode == HeatMode_User && _settings.calendarOn == 0)
		{
			powerLevel = _settings.powerLevel;
			if 			(powerLevel == 5)
				power_limit = 20;
			else if (powerLevel == 4)
				power_limit = 16;
			else if (powerLevel == 3)
				power_limit = 12;
			else if (powerLevel == 2)
				power_limit = 8;
			else if (powerLevel == 1)
				power_limit = 4;
			else
				power_limit = 0;
		}
		else
		{
			     if (power_current > 19)
				powerLevel = 5;
			else if (power_current > 15)
				powerLevel = 4;
			else if (power_current >= 10)
				powerLevel = 3;
			else if (power_current > 5)
				powerLevel = 2;
			else if (power_current > 0)
				powerLevel = 1;
			else
				powerLevel = 0;
			
			power_level_auto = powerLevel;
		}
		*/
/*		for (int i = 0; i < powerLevel; i++)
		{
			pxs.setColor(powerLevelColors[i]); 
			pxs.fillRectangle(11 + i * 53 + i * 8, 213, 53, 12);
		}

		pxs.setColor(MAIN_COLOR); 
		pxs.setBackground(BG_COLOR);
		pxs.setFont(Futura_Bold13a);
		
		
		
		if (_settings.heatMode == HeatMode_User && _settings.calendarOn == 0)
		{
			int8_t kerning[] = {2,2,2,2,2,-100};
			pxs.print(11, 194, "CUSTOM", kerning);
		}
		else
		{
			int8_t kerning[] = {2,2,2,-100};
			pxs.print(11, 194, "AUTO", kerning);
		}
		*/
		
		PowerPrint();
		
		pxs.setColor(RED_COLOR);
		pxs.fillOval(278, 16, 28, 28);
		
		pxs.setColor(MAIN_COLOR); 
		pxs.setBackground(BG_COLOR);
	}
	if (_settings.workMode != WorkMode_Turbo) DrawWifi();
  if (updater != 0x01)
	{
		smooth_backlight(1);
	}
}

void PowerPrint()
{
	if (_settings.workMode != WorkMode_Turbo && currentMenu == NULL)
	if ((power_current == 20)) // max power
	{
		pxs.setColor((_settings.workMode == WorkMode_Comfort) ? YELLOW_COLOR : GREEN_COLOR);
		pxs.fillRectangle(18, 200, 139, 15);
		if(_settings.powerLevel == 2)
		{				
			pxs.fillRectangle(163, 200, 139, 15);
		}
		else
		{
			pxs.setColor(GREY_COLOR);
			pxs.fillRectangle(163, 200, 139, 15);
		}
	}
	else
	{
		pxs.setColor(GREY_COLOR);
		pxs.fillRectangle(18, 200, 139, 15);
		pxs.fillRectangle(163, 200, 139, 15);
	}		
}

int16_t getModeTemperature()
{
	switch (_settings.workMode)
	{
		case WorkMode_Comfort:
			if(_settings.units) {histeresis_low = 2; histeresis_high = 6;}
			else {histeresis_low = 1; histeresis_high = 3;}
			return (_settings.units ? _settings.tempComfortF : _settings.tempComfort);
		case WorkMode_Eco:
			if(_settings.units) {histeresis_low = 4; histeresis_high = 2;}
			else {histeresis_low = 2; histeresis_high = 1;}			
			return (_settings.units ? _settings.tempEcoF : _settings.tempEco);
		case WorkMode_Turbo:
			return 30;
		default:
			return 0;
	}
}

WorkMode getCalendarMode()
{
	rtc_current_time_get(&rtc_initpara); 
	struct Presets* _pr = (_settings.calendar[rtc_initpara.rtc_day_of_week - 1] < 7) ? (struct Presets*)&_presets[_settings.calendar[rtc_initpara.rtc_day_of_week - 1]] : &_settings.custom;
	return (WorkMode)_pr->hour[bcdToDec(rtc_initpara.rtc_hour)];
}

uint8_t getCalendartemp()
{
	rtc_current_time_get(&rtc_initpara); 
	struct Presets* _pr = (struct Presets*)&_settings.week_schedule[rtc_initpara.rtc_day_of_week - 1];
	return _pr->hour[bcdToDec(rtc_initpara.rtc_hour)];
}


void InitTimer()
{
	if (_settings.on == 0 || (_settings.timerOn == 0 && _settings.calendarOn == 0 && _settings.workMode != WorkMode_Turbo))
	{
		rtc_alarm_disable();
		delay_1ms(200);
		return;
	}
	rtc_current_time_get(&rtc_initpara); 
	
	if (bcdToDec(rtc_initpara.rtc_year) < 23 && _settings.calendarOn)
	{
		// disable timer and calendar
		_settings.timerOn = 0;
		_settings.calendarOn = 0;
		_settings.workMode = WorkMode_Comfort;
		rtc_alarm_disable();
		delay_1ms(200);
		return;
	}	
	
	rtc_alarm_disable(); 
//-----------------------------------------------------	
	if (_settings.timerOn == 1 || _settings.workMode == WorkMode_Turbo)
	{
		
    rtc_alarm.rtc_alarm_hour = 0x00;
    rtc_alarm.rtc_alarm_minute = 0x00;
    rtc_alarm.rtc_alarm_second = rtc_initpara.rtc_second;
		rtc_alarm.rtc_alarm_mask = RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_DATE_MASK;
		
		rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
		rtc_alarm.rtc_alarm_day = RTC_WEDSDAY;
		rtc_alarm_config(&rtc_alarm);
		rtc_flag_clear(RTC_STAT_ALRM0F);
		//rtc_interrupt_enable(RTC_INT_ALARM);  
		rtc_alarm_enable();		
	}
	else if (_settings.calendarOn == 1)
	{
		
    rtc_alarm.rtc_alarm_hour = 0x00;
    rtc_alarm.rtc_alarm_minute = 0x00;
    rtc_alarm.rtc_alarm_second = 0x00;
		rtc_alarm.rtc_alarm_mask = RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_SECOND_MASK | RTC_ALARM_DATE_MASK;
		
		
		rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
		rtc_alarm.rtc_alarm_day = RTC_WEDSDAY;
		rtc_alarm_config(&rtc_alarm);
		rtc_flag_clear(RTC_STAT_ALRM0F);
		//rtc_interrupt_enable(RTC_INT_ALARM);  
		rtc_alarm_enable();
		
		//alarm_set(0);
	}	
	
	delay_1ms(200);
}

void ResetAllSettings()
{
	_settings.on = 1;
	_settings.blocked = 0;
	_settings.tempComfort = 30;
	_settings.tempEco = 25;
	_settings.tempComfortF = 86;
	_settings.tempEcoF = 77;
	_settings.tempAntifrost = 5;
	_settings.calendarOn = 0;
	_settings.brightness = 1;
	_settings.lowScreenMode = 0;
	_settings.soundOn = 1;
	_settings.displayAutoOff = 0;
	_settings.half_power = 0;
	_settings.heatMode = HeatMode_Auto;
	_settings.powerLevel = 1;
	_settings.workMode = WorkMode_Turbo;
	_settings.tmp_workMode = WorkMode_Comfort;
	_settings.modeOpenWindow = 0;
	_settings.units = 1;
	_settings.calendar[0] = 3;
	_settings.calendar[1] = 3;
	_settings.calendar[2] = 3;
	_settings.calendar[3] = 3;
	_settings.calendar[4] = 3;
	_settings.calendar[5] = 3;
	_settings.calendar[6] = 3;
	//memset(&_settings.week_schedule, 5, sizeof(_settings.week_schedule));
	//_settings.week_schedule[0].hour[3] = 30;
	//_settings.week_schedule[0].hour[4] = 10;
	/*
	 _settings.week_schedule[7] = {
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 }
  };
	*/
	
	//memset(&_settings.custom, pEco, sizeof(_settings.custom));
	_settings.timerOn = 0;
	_settings.timerTime = 0; // 0H
	memset(&_settings.UDID, 0, sizeof(_settings.UDID));
	
	rtc_setup();
}

void blocked(uint8_t updater)
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
	pxs.clear(); 

	if (pxs.sizeCompressedBitmap(width, height, img_blocked_png_comp) == 0)
		pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2 - (updater ? 30 : 0), img_blocked_png_comp);
	if(updater)
	{
	pxs.setFont(FuturaBookC20a);
	DrawTextAligment(160 - (pxs.getTextWidth("Long press") /2), 240 / 2 + pxs.getTextLineHeight() +10 , pxs.getTextWidth("Long press"), pxs.getTextLineHeight(), "Long press", false, false);
	DrawTextAligment(160 - (pxs.getTextWidth("< > to unlock") /2), 240 / 2 + pxs.getTextLineHeight()+40 , pxs.getTextWidth("< > to unlock"), pxs.getTextLineHeight(), "< > to unlock", false, false);
	}
	smooth_backlight(1);
	delay_1ms(2000);
	if(_settings.on)
	{
		DrawMainScreen();
	}
	else
	{
		smooth_backlight(0);
		pxs.clear();
	  pxs.displayOff();
	}
}

void unblocked()
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
	pxs.clear(); 

	if (pxs.sizeCompressedBitmap(width, height, img_unblocked_png_comp) == 0)
		pxs.drawCompressedBitmap(320 / 2 - width / 2, 240 / 2 - height / 2, img_unblocked_png_comp);
	pxs.setColor(BG_COLOR);
	pxs.fillRectangle(320 / 2+3, 240 / 2 - 70 , 100, 45);
	pxs.setColor(MAIN_COLOR);
	smooth_backlight(1);
	delay_1ms(2000);
	if(_settings.on) 
	{
		DrawMainScreen();
	}
	else
	{
		smooth_backlight(0);
		pxs.clear();
	  pxs.displayOff();
	}
}

void startScreen()
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
	
	if(_settings.brightness) _stateBrightness = StateBrightness_ON;
	else _stateBrightness = StateBrightness_LOW;

	
	pxs.clear(); 
	pxs.displayOn();
	
	smooth_backlight(0);
	pxs.clear();
 	pxs.drawCompressedBitmap(40, 50, img_logo_png_comp);
	pxs.drawCompressedBitmap(40, 155, img_text_logo_png_comp);
	//pxs.setFont(FuturaMediumC57a);
	//int16_t width_t = pxs.getTextWidth("Ballu");
	//pxs.print(320 / 2 - width_t/2, 240/2 - 40, "Ballu");	
	//pxs.setColor(powerLevelColors[4]); 
	//pxs.setFont(FuturaMediumC27a);

	//int16_t width_t = pxs.getTextWidth("EUROTECH METAL");
	//pxs.print(320 / 2 - width_t/2, 240/2 - 40, "EUROTECH METAL");
	//width_t = pxs.getTextWidth("PANEL HEATER");
	//pxs.print(320 / 2 - width_t/2, 240/2, "PANEL HEATER");
	
	//pxs.setFont(FuturaBookC20a);
	//pxs.setColor(MAIN_COLOR); 
	//char ver_buffer[20];
	//sprintf(ver_buffer, "%s%s", "V. ", VERSION);	
	//width_t = pxs.getTextWidth(ver_buffer);
	//pxs.print(320/2 - width_t/2, 185, ver_buffer);
  smooth_backlight(1);
	delay_1ms(3000);
	smooth_backlight(0);
	currentMenu = NULL;
	nextChangeLevel = 0;
	if (_settings.calendarOn == 1)
	{
		_settings.workMode = getCalendarMode();
	}
	
	temp_current = getTemperature();
	if (temp_current  == -127)
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 1;
		/*
		if(!_error_fl)
		{
			_error_fl = 1;
			pxs.clear();
			pxs.setFont(FuturaBookC90a);
			DrawTextAligment(0, 0, SW, SH, "E1", false, false);
			//smooth_backlight(1);
			//query_faults();
		}*/
		
	}
	else if (temp_current  == 127)
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 2;
		/*
		if(!_error_fl)
		{
			_error_fl = 1;
			pxs.clear();
			pxs.setFont(FuturaBookC90a);
			DrawTextAligment(0, 0, SW, SH, "E2", false, false);
			//smooth_backlight(1);
			//query_faults();
		}*/
	}
	else if (temp_current > (_settings.units ? 118 : 48))
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 3;
		/*
		if(!_error_fl)
		{
			_error_fl = 1;
			pxs.clear();
			pxs.setFont(FuturaBookC90a);
			DrawTextAligment(0, 0, SW, SH, "E3", false, false);
			//smooth_backlight(1);
			//query_faults();
		}*/
	}
	else if (temp_current < (_settings.units ? -15 : -26))
	{
		SetPower(0);
		power_level_auto = 0;
		_error = 4;
		/*
		if(!_error_fl)
		{
			_error_fl = 1;
			pxs.clear();
			pxs.setFont(FuturaBookC90a);
			DrawTextAligment(0, 0, SW, SH, "E4", false, false);
			//smooth_backlight(1);
			//query_faults();
		}*/
	}
	else
	{
		DrawMainScreen();
	}
}

void deviceON()
{
	//smooth_backlight(1);
	//fwdgt_counter_reload();
	_settings.on = 1;
	startScreen();
	_error_fl = 0;
	_settings.timerOn = 0;
	_settings.timerTime = 0;
	timer_time_set = 0;
	InitTimer();
	_timeoutSaveFlash = GetSystemTick();
	_timerStart = GetSystemTick();
	
	if (_settings.heatMode == HeatMode_Auto)
		SetPower(10);
}

void deviceOFF()
{	
	
	_settings.timerOn = 0;
	timer_time_set = 0;	
	_settings.timerTime = 0;
	SetPower(0);
	_settings.on = 0;
	heat_from_cold = 1;
  open_window_counter = 0;
	open_window_temp_main_start = 255;
	window_was_opened = 0;
	window_is_opened = 0;
	smooth_backlight(0);
	pxs.displayOff();
	pxs.clear();
	query_settings();
	//InitTimer();
	_timeoutSaveFlash = GetSystemTick();
}

bool keyPressed()
{
	bool result = true;
	
	if ((_stateBrightness == StateBrightness_OFF) && _settings.on)
	{
		beep();
		result = false;
	}	
	
	idleTimeout = GetSystemTick();
	
	if (_settings.blocked && !_settings.on)
	{
		pxs.displayOn();
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		beep();
		smooth_backlight(0);
		blocked(1);
		return false;
	}

	if (_settings.blocked && _settings.on)
	{
		if((_stateBrightness == StateBrightness_LOW || (_stateBrightness == StateBrightness_ON))) beep();
		pxs.displayOn();
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		smooth_backlight(0);
		blocked(1);
		idleTimeout = GetSystemTick() - 27000;
		return false;
	}
	
	if(_settings.lowScreenMode && _settings.on)
	{
		beep();
		_settings.lowScreenMode = 0;
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		smooth_backlight(0);	
		DrawMainScreen();
		query_settings();
		_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
		return false;
	}
	
	
	if (!_settings.blocked && _settings.on)
	{

		
		if((_settings.displayAutoOff) && (_stateBrightness == StateBrightness_OFF))
		{
			if(_settings.brightness)
			{
				_stateBrightness = StateBrightness_ON;
			}
			else 
			{
				_stateBrightness = StateBrightness_LOW;
			}
			DrawMainScreen();
			nextChangeLevel = GetSystemTick();
		}
		pxs.displayOn();
		if(currentMenu == NULL)
		{

			//smooth_backlight(1);
	  }
	}
	
	nextChangeLevel = GetSystemTick();
	return result;	
	
}


uint8_t bcdToDec(uint8_t val)
{
  return( (val/16*10) + (val%16) );
}

uint8_t decToBcd(uint8_t val)
{
  return( (val/10*16) + (val%10) );
}

/*bool f_open_window (int8_t temp_current, uint8_t power_current)
{
    //Set current temp to compare and increase if current temp is going higher up to requested

    if (open_window_temp_main_start == 255)                                             // If we change operating mode = 255
    {
			open_window_temp_main_start = temp_current;
		}

    if ((power_current == power_limit) && (open_window_temp_main_start > temp_current) && (open_window_counter != open_window_times)) // if we loose warm when heating
    {
        open_window_counter++;
    }
		
    if ((temp_current <= (_settings.tempAntifrost - histeresis_low)) // if we reached af mode when temp falling down
		  && _settings.workMode != WorkMode_Antifrost 									// if it was not af mode
		  && open_window_temp_main_start > temp_current)								// if we loose warm
    {
			open_window_counter = 0;
			open_window_temp_main_start = 255;	
			_settings.modeOpenWindow = 0;
			_settings.workMode = WorkMode_Antifrost;
			window_was_opened = 1;
			_settings.calendarOn = 0;
			_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
			DrawMainScreen();
			return true;
    }

    if ((open_window_temp_main_start < temp_current) && (temp_current <= (getModeTemperature() - histeresis_low)) && (open_window_counter != open_window_times)) // ???????????? open_window_temp_start ??? ????? ???????????
    {
        open_window_temp_main_start = temp_current;
        open_window_counter = 0;
    }
       
    //Return true if temp goes down x times and open window state achieved
    if (open_window_counter == open_window_times)
        return true;
    else
        return false;
}

void open_window_func()
{
	if (currentMenu == NULL && !_error)
	{
		if(!window_is_opened)
		{			
			if(_settings.modeOpenWindow)
			{
				pxs.drawCompressedBitmap(12, 140, (uint8_t*)img_icon_open_png_comp);
			}
			else if(!_settings.modeOpenWindow)
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(12, 140, 50, 48);
				pxs.setColor(MAIN_COLOR);
			}			
		}
		else
		{
			if(!window_was_opened) // if not antifrost when open window
			{					
				open_window_counter = 0;
				open_window_temp_main_start = 255;
		
				window_is_opened = 0;
				DrawMainScreen();
			}
			else
			{
				window_was_opened = 0;
				window_is_opened = 0;
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(12, 140, 50, 48);
				pxs.setColor(MAIN_COLOR);
			}
		}		
	}	
}
*/
void set_watchdog()
{
		rcu_osci_on(RCU_IRC40K);
	  while(ERROR == rcu_osci_stab_wait(RCU_IRC40K));
	  fwdgt_config(0x0FFF, FWDGT_PSC_DIV128);
    fwdgt_enable();
}



void change_set_temp(uint8_t direction)
{
		volatile int16_t temp_set = getModeTemperature();
		CleanTemperature(getModeTemperature(),0,15);

		direction ? temp_set++ : temp_set--;

		if (temp_set > (_settings.units ? 86 : 30))
		{
			temp_set = _settings.units ? 50 : 10;
		}
		else if (temp_set < (_settings.units ? 50 : 10))
		{
			temp_set = (_settings.units ? 86 : 30);
		}
		
		if(_settings.workMode == WorkMode_Comfort)
		{
			_settings.units ? _settings.tempComfortF = temp_set : _settings.tempComfort = temp_set;
		}
		else
		{
			_settings.units ? _settings.tempEcoF = temp_set : _settings.tempEco = temp_set;
		}
		
		_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
		DrawTemperature(getModeTemperature(),0,15);	
}

void loop(void)
{
	uint8_t* p = (uint8_t*)&_settings;
	memcpy(p, (uint8_t*)SETTINGSADDR, sizeof(_settings));
  if (_settings.crc != crc32_1byte(p, offsetof(DeviceSettings, crc), 0xFFFFFFFF)) // not valid crc
		ResetAllSettings();
	
  timer_time_set = _settings.timerTime * 60;

	pxs.setOrientation(LANDSCAPE);
	pxs.enableAntialiasing(true);
	pxs.init();
	pxs.clear(); 
	
	_settings.units ? temp_steinhart = 77 :  temp_steinhart = 25;
	getTemperature();
	getTemperature();
	getTemperature();
	getTemperature();
	getTemperature();
	
	if (_settings.on)
	{
		startScreen();
		_timerStart = GetSystemTick();
		
	}
	else
	{
		pxs.displayOff();
	}
	
	if(_settings.workMode == WorkMode_Turbo)
	{
		_settings.powerLevel = 2;
		timer_time_set = TIMER_TURBO;
		_settings.timerOn = 0;
		_settings.timerTime = 0;
		InitTimer();
	}

	if (_settings.heatMode == HeatMode_Auto)
		SetPower(0);
	
  idleTimeout = GetSystemTick();
  set_watchdog();
	
	while (1)
  {
		// short push is working from release the finger. long press work standart. See ClickButton.h
			_key_window.update();
			_key_power.update();
			_key_menu.update();
			_key_back.update();
			_key_down.update();
			_key_up.update();
		
		fwdgt_counter_reload();
		
		
//---------------------------------------------- ALARM CALLBACK 		
		if(RESET != rtc_flag_get(RTC_STAT_ALRM0F))
		{
			rtc_flag_clear(RTC_STAT_ALRM0F);
			rtc_alarm_callback();
		}
//-------------------------------------------------------------

//------------------------------------------------- UART HANDLE
    receive_uart_int();
//-------------------------------------------------------------
		
		
		if (_timeoutSaveFlash != 0 && GetSystemTick() >= _timeoutSaveFlash)
		{
			_timeoutSaveFlash = 0;
			SaveFlash();
			query_settings();
		}		

		
		if (_key_power.getPressed() && !_key_window.getState() && !_key_menu.getState() && !_key_back.getState() && !_key_up.getState() && !_key_down.getState())
		{
			if (!keyPressed())
				continue;
	
			beep();
				
			if (_settings.on)
				deviceOFF();
			else
				deviceON();	
			
		}

		if (_key_down.getState() && _key_up.getState() && ((!_error && _settings.on) || (!_settings.on)))
		{
			if (_key_down.duration() > 2000 && _key_up.duration() > 2000)
			{
				beep();
				_settings.blocked = !_settings.blocked;
				pxs.displayOn();
				idleTimeout = GetSystemTick();
				if(_settings.brightness) _stateBrightness = StateBrightness_ON;
				else _stateBrightness = StateBrightness_LOW;

				smooth_backlight(0);
				if (_settings.blocked)
					blocked();
				else
					unblocked();
				//
				
				_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;				  

				_key_down.getPressed();
				_key_up.getPressed();
				_key_down.getLongPressed();
				_key_up.getLongPressed();
			}
			else {
				_key_down.getPressed();
				_key_up.getPressed();
				_key_down.getLongPressed();
				_key_up.getLongPressed();
				
				continue;
			}
		}
		else if (_key_down.getPressed() && !_key_power.getState() && !_key_menu.getState() && !_key_back.getState() && !_key_up.getState() && !_key_window.getState() && !_error)
		{
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;

			if (!_key_down.isLongPressed())
				beep();
			
			if (currentMenu != NULL)
			{
				MenuPrev();
			}
			else
			{
				uint32_t updater = 0x01;

				if (_settings.workMode == WorkMode_Turbo)
				{
					//_settings.tempComfort = getModeTemperature();
					_settings.workMode = _settings.tmp_workMode;
					timer_time_set = 0;
					InitTimer();
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					DrawMainScreen(updater);
				}
				else
				{
					change_set_temp(0);
				}
			}
		}
		else if (_key_up.getPressed()&& !_key_power.getState() && !_key_menu.getState() && !_key_back.getState() && !_key_window.getState() && !_key_down.getState() && !_error)
		{
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;
			if (!_key_up.isLongPressed())
				beep();
			

			
			if (currentMenu != NULL)
			{
				MenuNext();
			}
			else
			{
				uint32_t updater = 0x01;

				if (_settings.workMode == WorkMode_Turbo)
				{
					//_settings.tempComfort = getModeTemperature();
					_settings.workMode = _settings.tmp_workMode;
					timer_time_set = 0;
					InitTimer();
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					DrawMainScreen(updater);
				}
				else
				{
					change_set_temp(1);
				}
			}
		}


		if (_key_window.getPressed() && !_key_power.getState() && !_key_menu.getState() && !_key_back.getState() && !_key_up.getState() && !_key_down.getState() && !_error && (currentMenu == NULL))
		{	
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;
			beep();
			_settings.powerLevel = 2;
			if(_settings.workMode != WorkMode_Turbo)
			{
				_settings.tmp_workMode = _settings.workMode;
			}
			_settings.workMode = WorkMode_Turbo;
			_settings.timerOn = 0;
			_settings.timerTime = 0;
			timer_time_set = TIMER_TURBO;
			InitTimer();
			_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
		
			DrawMainScreen();
		}


		if (_key_menu.getPressed() && !_key_power.getState() && !_key_window.getState() && !_key_back.getState() && !_key_up.getState() && !_key_down.getState() && !_error)
		{
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;			
			beep();
			
			if (currentMenu != NULL)
			{
				MenuOK();
			}
			else
			{
				EnterMenu();
			}
		}
		
		if (_key_back.getLongPressed())
		{
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;	
			beep();	
			
			smooth_backlight(0);
			pxs.clear();
			pxs.setColor(RED_COLOR);
			pxs.fillOval(278, 16, 28, 28);
			pxs.setColor(MAIN_COLOR); 
			pxs.setBackground(BG_COLOR);				
			_stateBrightness = StateBrightness_LOW;
			smooth_backlight(1);
			_settings.lowScreenMode = 1;
			continue;
		}
		

		if (_key_back.getPressed() && !_key_power.getState() && !_key_menu.getState() && !_key_window.getState() && !_key_up.getState() && !_key_down.getState() && !_error)
		{
			if(!_settings.on)
				continue;				
			if (!keyPressed())
				continue;		
			beep();
			
			if (currentMenu != NULL)
			{
				MenuBack();
			}
			else
			{
				if (_settings.workMode == WorkMode_Turbo)
				{
					//_settings.tempComfort = getModeTemperature();
					_settings.workMode = _settings.tmp_workMode;
					timer_time_set = 0;
					InitTimer();
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					
				}
				else if (_settings.workMode == WorkMode_Eco)
					_settings.workMode = WorkMode_Comfort;
				else if (_settings.workMode == WorkMode_Comfort)
					_settings.workMode = WorkMode_Eco;
			
				_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
			
				DrawMainScreen();
			}
		}

		if(!_settings.on)
			continue;		
			
		if (_key_power.getState() && _key_menu.getState() && _key_up.getState())
		{
			if (_key_power.duration() > 2000 && _key_menu.duration() > 2000 && _key_up.duration() > 2000)
			{
					beep();
					smooth_backlight(0);
					pxs.clear();
					pxs.setColor(RED_COLOR);
					pxs.fillOval(278, 16, 28, 28);
					pxs.setColor(MAIN_COLOR); 
					pxs.setBackground(BG_COLOR);				
					ResetAllSettings();
				
					pxs.setFont(FuturaBookC29a);
					char buffer[20];
					sprintf(buffer, "%s%s", "V.", VERSION);	
					DrawTextAligment(160 - (pxs.getTextWidth(buffer) / 2), 50, pxs.getTextWidth(buffer), pxs.getTextLineHeight(), buffer, false, false);	
				
					pxs.setFont(FuturaBookC29a);
					DrawTextAligment(160 - (pxs.getTextWidth("Restore default") /2), 240 / 2 , pxs.getTextWidth("Restore default"), pxs.getTextLineHeight(), "Restore default", false, false);
					DrawTextAligment(160 - (pxs.getTextWidth("settings") /2), 240 / 2 + pxs.getTextLineHeight() , pxs.getTextWidth("settings"), pxs.getTextLineHeight(), "settings", false, false);
				
					smooth_backlight(1);
					SaveFlash();
					delay_1ms(3000);
					NVIC_SystemReset();
			}
			else
			{
					_key_power.getPressed();
					_key_menu.getPressed();
					_key_up.getPressed();
					//_key_power.getLongPressed();
					//_key_menu.getLongPressed();
					//_key_up.getLongPressed();		
					continue;				
			}
		}			
			
	//==========================================================buttons end	
		
		if(refresh_system)
		{
			idleTimeout = GetSystemTick();
		}
		// auto switch off
		if (_settings.displayAutoOff && !_error && !window_is_opened)
		{
			if((GetSystemTick() > idleTimeout + 30000) && (_stateBrightness == StateBrightness_LOW))
			{
				_stateBrightness = StateBrightness_OFF;
				smooth_backlight(0);
			}
			else if ((GetSystemTick() > idleTimeout + 15000) && (_stateBrightness == StateBrightness_ON))
			{
				_stateBrightness = StateBrightness_LOW;
				smooth_backlight(1);
			}
		}

		if(GetSystemTick() > idleTimeout + 2000 && currentMenu != NULL && currentMenu->ID == 999)
		{
			MenuBack();
		}
		else if (GetSystemTick() > idleTimeout + 15000 && currentMenu != NULL)
		{
			MainScreen();
		}
		

		
		
//============================================================================= refrash display	
		if (((GetSystemTick() > nextChangeLevel) || (refresh_system)) && _settings.on)
		{	

			temp_current = getTemperature();
			int16_t modeTemp;
			if(_settings.calendarOn)
			{
				modeTemp = getCalendartemp();
				
				if (modeTemp != currentWorkMode)
				{
					CleanTemperature(currentWorkMode,0,15);
					currentWorkMode = modeTemp;
					DrawMainScreen(1);
				}
			}
			else
			{
				modeTemp = getModeTemperature();
			}
			
			power_current = _currentPower;
		
			
			if(!DrawError())
			{
				_error = 0;
				if(_error_fl)
				{
					_error_fl = 0;
					_settings.units ? temp_steinhart = 77 :  temp_steinhart = 25;

					DrawMainScreen();
				}

				if(_settings.workMode != WorkMode_Turbo)
				{
					if (temp_current  <= (modeTemp - histeresis_low))
						power_current = power_limit;
					else if (temp_current >= (modeTemp + histeresis_high))
						power_current = 0;	
				}
				else
					power_current = power_limit;
				
				SetPower(power_current);		
				
				if(!_settings.lowScreenMode) PowerPrint();
				
				if(refresh_system)
				{
					idleTimeout = GetSystemTick();
					refresh_system = false;
				}
				else
				{
					query_settings();
					nextChangeLevel = GetSystemTick() + 60000;			
				}					
			}
		}
		
//========================================================= refrash 1 sec		
		if (GetSystemTick() > refrash_time && _settings.on)
		{	
			//
			
			if(currentMenu->ID == 4431)
			{
				static uint8_t connstep = 1;
				pxs.setFont(FuturaBookC29a);
				switch (connstep) {				
					case 1:
					  pxs.print(235, 90, ".");
						break;
					case 2:
						pxs.cleanText(235, 90, ".");
					  pxs.print(235, 90, "..");
						break;
					case 3:
						pxs.cleanText(235, 90, "..");
					  pxs.print(235, 90, "...");
						break;
					case 4:
					  pxs.cleanText(235, 90, "...");
						break;						
					
				}
				if(++connstep == 5) connstep = 1;
			}
			
			if (currentMenu == NULL && !_error)
			{
				#ifdef DEBUG	
				char buffer[10];
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(225, 20, 75, 20);
				pxs.setColor(MAIN_COLOR);		
				sprintf(buffer, "%d", getTemperature());
				
				pxs.setFont(FuturaBookC20a);
				DrawTextAligment(235, 20, 30, 20, buffer, false);	
				#endif
			}
			
//============================================================ Calendar & Timer		
	
			if (_eventTimer) // timer event
			{
				if (_settings.timerOn == 1)
				{
					deviceOFF();
					//continue;
				}
				else
				{
					_settings.workMode = _settings.tmp_workMode;
					timer_time_set = 0;
					InitTimer();
					DrawMainScreen();
					nextChangeLevel = GetSystemTick() + 1000;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					//uint8_t currentWorktemp = getCalendartemp();
					//if (modeTemp != currentWorkMode)
				}
				_eventTimer = 0;
			}
//=============================================================================================
			if(_settings.timerOn)
			{
				query_timer();
			}				

			refrash_time = GetSystemTick() + 1000;
		}
//=============================================================================================		
		if (currentMenu == NULL && !_error && _settings.workMode != WorkMode_Turbo && !_settings.lowScreenMode)
		{
			DrawWifi();
		}
	}
}

