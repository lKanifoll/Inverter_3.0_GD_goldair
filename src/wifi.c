
#include "wifi.h"
#include "string.h"
#include "inverter.h"
#include "stream.h"

extern void SaveFlash();
extern void InitTimer();
extern void DrawMainScreen(uint32_t updater);
extern void deviceOFF();
extern void deviceON();


extern bool refresh_system;
extern struct DeviceSettings _settings;
extern uint8_t power_level_auto;	
extern uint16_t timer_time_set;
extern uint8_t _eventTimer;
extern StateBrightness _stateBrightness;
extern uint32_t idleTimeout;
extern uint32_t _timeoutSaveFlash;
extern uint8_t _error;
extern uint8_t _currentPower;
uint16_t rxcount = 0;
uint8_t idle_count = 0;
uint8_t crc = 0;
uint8_t answer_cmd[50] = {0x55, 0xAA, 0x03};
uint8_t answer_cmd1[50] = {0x55, 0xAA, 0x03};
uint8_t prod_info[] = "{\"p\":\"hgbiso5ljxskxtde\",\"v\":\"2.1.1\",\"m\":0}"; // Amazon
//uint8_t prod_info[] = "{\"p\":\"xr6jsgylldbpkaz9\",\"v\":\"1.1.1\",\"m\":0}"; // GoldAir
//uint8_t prod_info[] = "{\"p\":\"6bbwxfx9leraqht1\",\"v\":\"1.1.1\",\"m\":0}";
//uint8_t prod_info[] = "{\"p\":\"jxe9szwafgw47a4h\",\"v\":\"1.1.1\",\"m\":0}";
extern uint8_t wifi_status;
uint8_t idle_flag_stat = 0;	
uint8_t recv_buffer[200];
void receive_uart_int(void);
uint8_t answer_out[300];
uint8_t datetime_arr[8];
Stream answer_frame(answer_out, 300);
//uint8_t recv_buffer_compl[255];

uint8_t chksum8(const uint8_t *buff, size_t len)
{
    uint8_t sum;       
    for ( sum = 0 ; len != 0 ; len-- )
        sum += *(buff++); 
		
    return sum;
}

void usart_transmit_frame(const uint8_t *buff, size_t len)
{
	for(uint16_t i = 0; i < len; i++)
	{
		usart_data_transmit(USART1, buff[i]);
		while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
	}
}

		uint16_t payload_len = 0;
		uint8_t  frame_cmd   = 0;
		uint8_t  device_cmd  = 0;

void receive_uart_int()
{

	if(idle_flag_stat)
	{
		// PARSER
		uint16_t  pointer     = 0;

		while(pointer != rxcount)
		{
			if(recv_buffer[pointer] == HEADER_1B && recv_buffer[pointer+1] == HEADER_2B) // FIND HEADER 55AA
			{
				usart_interrupt_disable(USART1, USART_INT_RBNE);
				//pointer += HEADER_LEN;
				//pointer++;
				frame_cmd = recv_buffer[pointer+3];
				if(frame_cmd  == CMD_INPUT)
				{
					device_cmd = recv_buffer[pointer+6];
				}
				payload_len = ((recv_buffer[pointer+4] << 8) | recv_buffer[pointer+5])  ;
				
				uint8_t *frame = new uint8_t[payload_len + 7];
				
				for(uint8_t ii = 0; ii < (payload_len + 7); ii++)
				{
					frame[ii] = recv_buffer[pointer + ii];								
				}
				
				crc = chksum8(frame, payload_len + 6);
				
				if(crc == frame[payload_len + 6])
				{
					answer_frame.clear();
					answer_frame.reset();
					answer_frame.put(HEADER_1B);
					answer_frame.put(HEADER_2B);
					answer_frame.put(HEADER_VER);
					
					
					if(frame_cmd == CMD_HB)
					{
						answer_frame.put(CMD_HB);
						answer_frame.put(0x00);
						answer_frame.put(0x01);
						answer_frame.put(HEARTBEAT);
						answer_frame.put(chksum8(answer_frame.sptr(), 7));
						usart_transmit_frame(answer_frame.sptr(), 8);
						//query_settings();

					}
					if(frame_cmd == CMD_INFO)
					{
						answer_frame.put(CMD_INFO);
						answer_frame.put(0x00);
						answer_frame.put(0x2A);
						answer_frame.put_str(prod_info,0x2A);
						answer_frame.put(chksum8(answer_frame.sptr(), 0x2A+6));
						usart_transmit_frame(answer_frame.sptr(), 0x2A+7);		
					}
					if(frame_cmd == CMD_WMODE)
					{
						answer_frame.put(CMD_WMODE);
						answer_frame.put(0x00);
						answer_frame.put(0x00);
						answer_frame.put(chksum8(answer_frame.sptr(), 6));
						usart_transmit_frame(answer_frame.sptr(), 7);
					}			
					if(frame_cmd == CMD_WF_STAT)
					{
						wifi_status = frame[6];
						answer_frame.put(CMD_WF_STAT);
						answer_frame.put(0x00);
						answer_frame.put(0x00);
						answer_frame.put(chksum8(answer_frame.sptr(), 6));
						usart_transmit_frame(answer_frame.sptr(), 7);
/*
						answer_cmd[3] = CMD_NET_CONF;
						answer_cmd[4] = 0x00;
						answer_cmd[5] = 0x01;
						answer_cmd[6] = 0x00;
						answer_cmd[7] = chksum8(answer_cmd, 7);
						usart_transmit_frame(answer_cmd, 8);		

						answer_cmd[3] = CMD_RESET;
						answer_cmd[4] = 0x00;
						answer_cmd[5] = 0x00;
						//memmove(answer_cmd+6, prod_info, 0x2A);
						//answer_cmd[6] = 0x00;
						answer_cmd[6] = chksum8(answer_cmd, 6);
						usart_transmit_frame(answer_cmd, 7);	*/			
					}			
					if(frame_cmd == CMD_QUERY)
					{ 
						query_settings();			
					}
					if(frame_cmd == CMD_DATETIME)
					{
						
						if(frame[6])
						{/*
							datetime_arr[0] = decToBcd(frame[7]);
							datetime_arr[1] = decToBcd(frame[8]);
							datetime_arr[2] = decToBcd(frame[9]);
							datetime_arr[3] = decToBcd(frame[10]);
							datetime_arr[4] = decToBcd(frame[11]);
							datetime_arr[5] = decToBcd(frame[12]);
							datetime_arr[6] = frame[13];
							*/
							rtc_deinit();
							rtc_parameter_struct rtc_from_module;
							
							rtc_from_module.rtc_year = decToBcd(frame[7]);
							rtc_from_module.rtc_month = decToBcd(frame[8]);
							rtc_from_module.rtc_date = decToBcd(frame[9]);
							rtc_from_module.rtc_hour = decToBcd(frame[10]);
							rtc_from_module.rtc_minute = decToBcd(frame[11]);
							rtc_from_module.rtc_second = 0;//decToBcd(frame[12]);
							rtc_from_module.rtc_day_of_week = frame[13];
							
							rtc_from_module.rtc_factor_asyn = 0x7FU;
							rtc_from_module.rtc_factor_syn = 0xFFU;
							rtc_from_module.rtc_display_format = RTC_24HOUR;	
							
							rtc_init(&rtc_from_module);
						}
					}				
					
					if(frame_cmd == CMD_INPUT)
					{
						if(device_cmd == ID_SWITCH)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_SWITCH);
							answer_frame.put(1);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
	
							if(_settings.on != frame[10])
							{
								_settings.on = frame[10];
								
								if (_settings.on)
									deviceON();
								else
									deviceOFF();
							}
					    query_settings();
						}
						
						if(device_cmd == ID_CHILDLOCK)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_CHILDLOCK);
							answer_frame.put(1);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
							
							if(_settings.blocked != frame[10])
							{
								_settings.blocked = frame[10];
							}	
							idleTimeout = GetSystemTick();
							if(_settings.brightness) _stateBrightness = StateBrightness_ON;
							else _stateBrightness = StateBrightness_LOW;

							smooth_backlight(0);
							if (_settings.blocked)
								blocked();
							else
								unblocked();							
						}	
						if (_settings.on){
						if(device_cmd == ID_WORKMODE)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_WORKMODE);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	
							
							//if( ((WorkMode)frame[10] == WorkMode_Turbo) && ( (_settings.workMode == WorkMode_Comfort) || (_settings.workMode == WorkMode_Eco)) )
							if(_settings.workMode != WorkMode_Turbo)
							{
								_settings.tmp_workMode = _settings.workMode;
							}
							_settings.workMode = (WorkMode)frame[10];
							if(_settings.workMode == WorkMode_Turbo)
							{
								_settings.powerLevel = 2;
								_settings.timerOn = 0;
								_settings.timerTime = 0;
								timer_time_set = TIMER_TURBO;
								InitTimer();
							}
							else
							{
								if(!_settings.timerOn) 
								{
									timer_time_set = 0;
									InitTimer();
								}
							}
							
							DrawMainScreen();
							refresh_system = true;
						}
						
						if(device_cmd == ID_BRIGHT)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_BRIGHT);
							answer_frame.put(1);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
							
							if(_settings.lowScreenMode != frame[10])
							{
								_settings.lowScreenMode = frame[10];
								if(_settings.brightness) _stateBrightness = StateBrightness_ON;
								else _stateBrightness = StateBrightness_LOW;
								DrawMainScreen();
								refresh_system = true;	
							}			
						}		
						
						
						
						if(device_cmd == ID_CELS_SET)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_CELS_SET);
							answer_frame.put(2);
							answer_frame.put(0);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(frame[13]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	
							
							if(_settings.workMode == WorkMode_Turbo)
							{
								_settings.workMode = _settings.tmp_workMode;
								timer_time_set = 0;
							  InitTimer();
								DrawMainScreen();
							}
							else
							{
								if(_settings.workMode == WorkMode_Eco) 
								{
									CleanTemperature(_settings.tempEco,0,15);
									_settings.tempEco = frame[13];
								}
								if(_settings.workMode == WorkMode_Comfort) 
								{
									CleanTemperature(_settings.tempComfort,0,15);
									_settings.tempComfort = frame[13];
								}								
								DrawMainScreen(1);
							}
							
							refresh_system = true;
						}					

						if(device_cmd == ID_FAR_SET)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_FAR_SET);
							answer_frame.put(2);
							answer_frame.put(0);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(frame[13]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	

							if(_settings.workMode == WorkMode_Turbo)
							{
								_settings.workMode = _settings.tmp_workMode;
								timer_time_set = 0;
							  InitTimer();
								DrawMainScreen();
							}
							else
							{
								if(_settings.workMode == WorkMode_Eco) 
								{
									CleanTemperature(_settings.tempEcoF,0,15);
									_settings.tempEcoF = frame[13];
								}
								if(_settings.workMode == WorkMode_Comfort) 
								{
									CleanTemperature(_settings.tempComfortF,0,15);
									_settings.tempComfortF = frame[13];
								}								
								DrawMainScreen(1);
							}
							refresh_system = true;
						}									

						if(device_cmd == ID_FC_SET)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_FC_SET);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
							
							if(_settings.units != frame[10])
							{
								_settings.units = frame[10];
								DrawMainScreen();
							  refresh_system = true;								
							}			
						}
						

						if(device_cmd == ID_SOUND)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_SOUND);
							answer_frame.put(1);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
							
							if(_settings.soundOn != frame[10])
							{
								_settings.soundOn = frame[10];
							}			
						}
						
						if(device_cmd == ID_POWER_SET)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_POWER_SET);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);
							
							if(_settings.workMode == WorkMode_Turbo)
							{
								_settings.workMode = _settings.tmp_workMode;
								timer_time_set = 0;
							  InitTimer();
							}							
							
							//if((_settings.powerLevel - 1) != frame[10])
							{
								_settings.powerLevel = frame[10] + 1;
								DrawMainScreen();
							  refresh_system = true;					
							}			
						}		
						
						
						if(device_cmd == ID_TIMERTIME)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_TIMERTIME);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(1);
							answer_frame.put(frame[10]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	

							if(_settings.workMode == WorkMode_Turbo)
							{
								_settings.workMode = _settings.tmp_workMode;
								timer_time_set = 0;
							  InitTimer();
							}
							
							_settings.timerTime = frame[10];
							timer_time_set = frame[10] * 60;
							
							if(_settings.timerTime)
							{
								_eventTimer = 0;
								_settings.timerOn = 1;
								InitTimer();									
							}
							else
							{
								_eventTimer = 0;
								_settings.timerOn = 0;
								timer_time_set = 0;
								InitTimer();								
							}
							DrawMainScreen();
							refresh_system = true;
						}
						
						if(device_cmd == ID_CUSTOM_P)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_CUSTOM_P);
							answer_frame.put(2);
							answer_frame.put(0);
							answer_frame.put(4);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(frame[13]);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	
							
							_settings.powerLevel = frame[13];
							refresh_system = true;
						}	
						
						if(device_cmd == ID_SCHEDULE)
						{
							answer_frame.put(CMD_OUTPUT);
							answer_frame.put(frame[4]);
							answer_frame.put(frame[5]);
							answer_frame.put(ID_SCHEDULE);
							answer_frame.put(0);
							answer_frame.put(0);
							answer_frame.put(0xA8);
							answer_frame.put_str(&frame[10],168);
							answer_frame.put(chksum8(answer_frame.sptr(), payload_len+6));
							usart_transmit_frame(answer_frame.sptr(), payload_len+7);	
							memcpy(&_settings.week_schedule, frame+10, 168);
							refresh_system = true;
						}	
						}
						refresh_mainscreen();
						_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					}
				}
				delete []frame;	
			}
			pointer++;
		}
		//--------------
		rxcount = 0;
		//i = 0;
		idle_flag_stat = 0;
		usart_interrupt_enable(USART1, USART_INT_RBNE);
		//_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
	}
}

void reset_wifi_state()
{
		answer_frame.clear();
		answer_frame.reset();
		answer_frame.put(HEADER_1B);
		answer_frame.put(HEADER_2B);
		answer_frame.put(HEADER_VER);
		answer_frame.put(CMD_NET_CONF);
		answer_frame.put(0x00);
		answer_frame.put(0x01);
	  answer_frame.put(0x00);
		answer_frame.put(chksum8(answer_frame.sptr(), 7));
		usart_transmit_frame(answer_frame.sptr(), 8);	
		delay_1ms(10);
		answer_frame.clear();
		answer_frame.reset();
		answer_frame.put(HEADER_1B);
		answer_frame.put(HEADER_2B);
		answer_frame.put(HEADER_VER);
		answer_frame.put(CMD_RESET);
		answer_frame.put(0x00);
		answer_frame.put(0x00);
		answer_frame.put(chksum8(answer_frame.sptr(), 6));
		usart_transmit_frame(answer_frame.sptr(), 7);
}

void query_datetime()
{
	answer_frame.clear();
	answer_frame.reset();
	answer_frame.put(HEADER_1B);
	answer_frame.put(HEADER_2B);
	answer_frame.put(HEADER_VER);
	answer_frame.put(CMD_DATETIME);
	answer_frame.put(0x00);
	answer_frame.put(0x00);	
	answer_frame.put(chksum8(answer_frame.sptr(), 6));
	usart_transmit_frame(answer_frame.sptr(), 7);
}


void query_faults()
{
	answer_frame.clear();
	answer_frame.reset();
	answer_frame.put(HEADER_1B);
	answer_frame.put(HEADER_2B);
	answer_frame.put(HEADER_VER);
	answer_frame.put(CMD_OUTPUT);
	answer_frame.put(0x00);
	answer_frame.put(0x0A);
	//Fault
	answer_frame.put(ID_FAULT);
	answer_frame.put(5);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((_error == 1 ? 0x01 : 0x00) |
	                 (_error == 2 ? 0x04 : 0x00) |
	                 (_error == 3 ? 0x05 : 0x00) |
	                 (_error == 4 ? 0x08 : 0x00) );
	//Heat stat
	answer_frame.put(ID_HEAT_STAT);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((_currentPower ? 0 : 1));	
	
	answer_frame.put(chksum8(answer_frame.sptr(),10+6));
	usart_transmit_frame(answer_frame.sptr(), 10+7);
}



/*
#define ID_SWITCH			0x01
#define ID_CELS_SET		0x02
#define ID_CELS_CUR		0x03
#define ID_WORKMODE		0x04
#define ID_POWER_SET  0x05
#define ID_CHILDLOCK	0x07
#define ID_BRIGHT			0x0A
#define ID_HEAT_STAT	0x0B

#define ID_CURR_STAT	0x0C

#define ID_FC_SET			0x0D
#define ID_FAR_SET		0x0E
#define ID_FAR_CUR		0x0F

#define	ID_TIMERTIME	0x13
#define	ID_REMTIME		0x14
#define ID_FAULT			0x15
*/
extern uint32_t current_time;

void query_timer()
{
	answer_frame.clear();
	answer_frame.reset();
	answer_frame.put(HEADER_1B);
	answer_frame.put(HEADER_2B);
	answer_frame.put(HEADER_VER);
	answer_frame.put(CMD_OUTPUT);
	answer_frame.put(0x00);
	answer_frame.put(0x05);
	// timer time
	answer_frame.put(ID_TIMERTIME);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	
	if((timer_time_set % 60) == 0)
	{
		current_time = timer_time_set / 60;
		
		answer_frame.put( (uint8_t)current_time);	
		answer_frame.put(chksum8(answer_frame.sptr(),5+6));
		usart_transmit_frame(answer_frame.sptr(), 5+7);
	}
}


uint32_t current_temp = 0;

void query_settings()
{
	answer_frame.clear();
	answer_frame.reset();
	answer_frame.put(HEADER_1B);
	answer_frame.put(HEADER_2B);
	answer_frame.put(HEADER_VER);
	answer_frame.put(CMD_OUTPUT);
	answer_frame.put(0x00);
	answer_frame.put(0x58);
	
	//switch
	answer_frame.put(ID_SWITCH);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.on);


	//Set temperature
	answer_frame.put(ID_CELS_SET);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put((uint8_t)(_settings.workMode ? (_settings.workMode == WorkMode_Turbo ? 30 : _settings.tempComfort) : _settings.tempEco));// change!!!
	
	//Current temperature
	answer_frame.put(ID_CELS_CUR);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	
	current_temp = (_settings.units ? 30 : getTemperature());
	answer_frame.put(current_temp>>24);	
	answer_frame.put((current_temp>>16)&0xFF);
	answer_frame.put((current_temp>>8)&0xFFFF);
	answer_frame.put(current_temp&0xFFFFFF);
	

	//Working mode
	answer_frame.put(ID_WORKMODE);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((uint8_t)_settings.workMode);
	

	//Power set
	answer_frame.put(ID_POWER_SET);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	//answer_frame.put(_settings.workMode == WorkMode_Turbo ? 1 : _settings.powerLevel - 1);	
	answer_frame.put(_settings.powerLevel - 1);	
		
	//Child lock
	answer_frame.put(ID_CHILDLOCK);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.blocked);	
	
	
	//low screen mode
	answer_frame.put(ID_BRIGHT);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.lowScreenMode);	
	

	//Heat stat
	answer_frame.put(ID_HEAT_STAT);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((_currentPower ? 0 : 1));	
	
	//work stat
	answer_frame.put(ID_CURR_STAT);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(_currentPower ? (_settings.powerLevel == 1 ? 0x02 : 0x05) : 0);
	answer_frame.put(_currentPower ? (_settings.powerLevel == 1 ? 0xEE : 0xDC) : 0);
	
	
	//Far \ Cel
	answer_frame.put(ID_FC_SET);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.units);	
	
	//Far set
	answer_frame.put(ID_FAR_SET);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put((uint8_t)(_settings.workMode ? (_settings.workMode == WorkMode_Turbo ? 86 :_settings.tempComfortF) : _settings.tempEcoF)); 
	
	//Far curr
	answer_frame.put(ID_FAR_CUR);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	
	current_temp = (_settings.units ? getTemperature() : 86);
	answer_frame.put(current_temp>>24);	
	answer_frame.put((current_temp>>16)&0xFF);
	answer_frame.put((current_temp>>8)&0xFFFF);
	answer_frame.put(current_temp&0xFFFFFF);
	
	// timer time
	answer_frame.put(ID_TIMERTIME);
	answer_frame.put(4);
	answer_frame.put(0);
	answer_frame.put(1);
	
	if((timer_time_set % 60) == 0)
	{
		current_time = timer_time_set / 60;
	}
	
	answer_frame.put( (uint8_t)current_time);	

	//rem time
	answer_frame.put(ID_REMTIME);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	if(_settings.timerOn)
	{		
		answer_frame.put(timer_time_set>>24);	
		answer_frame.put((timer_time_set>>16)&0xFF);
		answer_frame.put((timer_time_set>>8)&0xFFFF);
		answer_frame.put(timer_time_set&0xFFFFFF);
	}
	else
	{
		answer_frame.put(0);	
		answer_frame.put(0);
		answer_frame.put(0);
		answer_frame.put(0);		
	}
	answer_frame.put(chksum8(answer_frame.sptr(),88+6));
	usart_transmit_frame(answer_frame.sptr(), 88+7);
	return;		
	//Fault
	answer_frame.put(ID_FAULT);
	answer_frame.put(5);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((_error == 1 ? 0x01 : 0x00) |
	                 (_error == 2 ? 0x04 : 0x00) |
	                 (_error == 3 ? 0x05 : 0x00) |
	                 (_error == 4 ? 0x08 : 0x00) );	
/*
	//Current power
	//answer_frame.put(ID_CURPOWER);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(_settings.heatMode ? _settings.powerLevel : power_level_auto);
	//Remining time
	answer_frame.put(ID_REMTIME);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(timer_time_set >> 8);
	answer_frame.put(timer_time_set);	
	//Fault
	answer_frame.put(ID_FAULT);
	answer_frame.put(5);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put((_error == 1 ? 0x01 : 0x00) |
	                 (_error == 2 ? 0x04 : 0x00) |
	                 (_error == 3 ? 0x05 : 0x00) |
	                 (_error == 4 ? 0x08 : 0x00) );

									 	
	//Schedule
	answer_frame.put(ID_SCHEDULE);
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(0xA8);
	answer_frame.put_str((uint8_t*)_settings.week_schedule,168);

	//Comfort temperature
	answer_frame.put(ID_COMFORT);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(_settings.tempComfort);	
	//ECO temperature
	answer_frame.put(ID_ECO);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(_settings.tempEco);		
	//Antifrost temperature
	answer_frame.put(ID_ANTIFR);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(_settings.tempAntifrost);	
	//Auto lcd off
	answer_frame.put(ID_LCDOFF);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.displayAutoOff);		
	//Programm
	answer_frame.put(ID_PROG);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.calendarOn);		
	//Sound
	answer_frame.put(ID_SOUND);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.soundOn);
	//Heat mode
	answer_frame.put(ID_HEATMODE);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.heatMode);		
	//Open window mode
	answer_frame.put(ID_OPENWINDOW);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.modeOpenWindow);
	//Timer
	answer_frame.put(ID_TIMER);
	answer_frame.put(1);
	answer_frame.put(0);
	answer_frame.put(1);
	answer_frame.put(_settings.timerOn);
	//Timertime
	answer_frame.put(ID_TIMERTIME);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(_settings.timerTime >> 8);
	answer_frame.put(_settings.timerTime);
	//custom power level
	answer_frame.put(ID_CUSTOM_P);
	answer_frame.put(2);
	answer_frame.put(0);
	answer_frame.put(4);
	answer_frame.put(0);	
	answer_frame.put(0);
	answer_frame.put(0);
	answer_frame.put(_settings.powerLevel);
*/
	
	answer_frame.put(chksum8(answer_frame.sptr(),94+6));
	usart_transmit_frame(answer_frame.sptr(), 94+7);
}
