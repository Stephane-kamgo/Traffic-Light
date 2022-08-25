#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <7seg.h>
#include <led.h>
														
typedef enum {
	STATE_START,
	AUTO_GELBROT,
	AUTO_GRUEN,
	FG_WARTEN,
	AUTO_GELB,
	AUTO_ROT,
	FG_GRUEN,
	FEHLERZUSTAND,
} state_t;

typedef enum {
	EVENT_BUTTON0,
	EVENT_BUTTON1,
	EVENT_ALARM,
} event_t;


static volatile uint8_t button0_event = 0;
static volatile uint8_t button1_event = 0;
static volatile uint8_t alarm_event = 0;
static volatile uint8_t overflow_counter = 0;
static volatile uint8_t num_7seg = 8;


static void init(void){
	//zeitgeber TIMER0 mit 8-Bit ZÃ¤hler, prescaler 1024,
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02) | (1<<CS00);


	//taster PD2 als Eingang (ative-low) mit pull-up
	//		INT0 bei fallender Flanke
	DDRD &= ~(1<<PD2);
	PORTD |= (1<<PD2);
	EICRA |= (1<<ISC01);
	EICRA &= ~(1<<ISC00);


	DDRD &= ~(1<<PD3);
	PORTD |= (1<<PD3);
	EICRA |= (1<<ISC11);
	EICRA &= ~(1<<ISC10);
	EIMSK |= (1<<INT1);

}

static void enable_BOTTON0(void){
	EIMSK |= (1<<INT0);
}
static void disable_BOTTON0(void){
	EIMSK &= ~(1<<INT0);
}

static void startAlarm(void){
	//ÃberlaufzÃ¤hler zurÃ¼cksetzen
	overflow_counter = 0;
	//Ãberlaufunterbrechung aktivieren / interrupt demaskieren
	TIMSK0 |= (1<<TOIE0);
}
static void stopAlarm(void){
	//Ãberlaufunterbrechung deaktivieren / maskieren
	TIMSK0 &= ~(1<<TOIE0);
}

static void setLEDstate(state_t state){
	switch(state){
		case STATE_START:
			sb_led_setMask(0);
			sb_led_on(RED0);
			sb_led_on(RED1);
			break;
		case AUTO_GELBROT:
			sb_led_setMask(0);
			sb_led_on(RED0);
			sb_led_on(RED1);
			sb_led_on(YELLOW0);
			break;
		case AUTO_GRUEN:
			sb_led_setMask(0);
			sb_led_on(GREEN0);
			sb_led_on(RED1);
			break;
		case FG_WARTEN:
			sb_led_setMask(0);
			sb_led_on(GREEN0);
			sb_led_on(RED1);
			sb_led_on(BLUE1);
			break;
		case AUTO_GELB:
			sb_led_setMask(0);
			sb_led_on(YELLOW0);
			sb_led_on(RED1);
			sb_led_on(BLUE1);
			break;
		case AUTO_ROT:
			sb_led_setMask(0);
			sb_led_on(RED0);
			sb_led_on(RED1);
			sb_led_on(BLUE1);
			break;
		case FG_GRUEN:
			sb_led_setMask(0);
			sb_led_on(RED0);
			sb_led_on(GREEN1);
			break;
		case FEHLERZUSTAND:

			sb_led_toggle(YELLOW0);
			break;
	}
}

ISR(INT0_vect){
	button0_event = 1;
}
ISR(INT1_vect){
	button1_event = 1;
}
ISR(TIMER0_OVF_vect){
	++overflow_counter;
	if(overflow_counter == 61){
		overflow_counter = 0;
		alarm_event = 1;
		num_7seg-- ;
	}
}


void main(void){
	//hardware initialisieren
	init();
	sei();
	//Sleepenable Bit setzen
	sleep_enable();

	const uint8_t time_reference = num_7seg;
	state_t state = STATE_START;
	startAlarm();
	setLEDstate(state);
	enable_BOTTON0();


	while(1){

		cli();
		while(button0_event == 0 && alarm_event == 0 && button1_event == 0){
			sei();
			sleep_cpu();
			cli();
		}

		event_t event;
		if(button0_event !=0){
			button0_event = 0;
			event = EVENT_BUTTON0;
		} else{
			 if(button1_event != 0){
				button1_event = 0;
				event = EVENT_BUTTON1;
			} else {
				alarm_event = 0;
				event = EVENT_ALARM;
			}
		}
		sei();

		state_t state_old = state;
		switch(state){
			case STATE_START:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else{
					if(event == EVENT_ALARM){
						state = AUTO_GELBROT;
					}else
					if(event == EVENT_BUTTON0){
						state = FG_WARTEN;
					}
				}
				break;
			case AUTO_GELBROT:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else{
					if(event == EVENT_ALARM){
						state = AUTO_GRUEN;
					}else
					if(event == EVENT_BUTTON0){
						state = FG_WARTEN;
					}
				}
				break;
			case AUTO_GRUEN:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else
				if(event == EVENT_BUTTON0){
					state = FG_WARTEN;
				}
				break;
			case FG_WARTEN:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else
				if((time_reference - num_7seg) == 5){
					state = AUTO_GELB;
				}

				break;
			case AUTO_GELB:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else
				if((time_reference - num_7seg) == 6){
					state = AUTO_ROT;
				}

				break;
			case AUTO_ROT:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else
				if((time_reference - num_7seg) == 8){
					state = FG_GRUEN;
				}

				break;
			case FG_GRUEN:
				if(event == EVENT_BUTTON1){
					state = FEHLERZUSTAND;
				}else
				if((time_reference - num_7seg) == 5){
					state = STATE_START;
				}
				break;
			case FEHLERZUSTAND:
				if(event == EVENT_BUTTON1){
					state = STATE_START;
				}
				break;
		}
		setLEDstate(state);
		if(state == state_old){
			if(state == FG_WARTEN || state == AUTO_GELB || state == AUTO_ROT){
				sb_7seg_showNumber(num_7seg);
			}
			continue;
		}


		switch(state){
			case STATE_START:
				EICRA &= ~(1<<ISC10);
				enable_BOTTON0();
				startAlarm();

				break;
			case AUTO_GELBROT:
				//startAlarm();

				break;
			case AUTO_GRUEN:
				stopAlarm();

				break;
			case FG_WARTEN:
				disable_BOTTON0();
				startAlarm();
				num_7seg = time_reference;
				sb_7seg_showNumber(num_7seg);

				break;
			case AUTO_GELB:
				//startAlarm();
				sb_7seg_showNumber(num_7seg);

				break;
			case AUTO_ROT:
				//startAlarm();
				sb_7seg_showNumber(num_7seg);

				break;
			case FG_GRUEN:
				num_7seg = time_reference;
				sb_7seg_disable();

				break;
			case FEHLERZUSTAND:
				EICRA |= (1<<ISC10);
				startAlarm();
				sb_led_setMask(0);
				setLEDstate(state);
				disable_BOTTON0();
				sb_7seg_disable();
				break;
		}
	}
}
