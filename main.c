/*
 * pulse_sensor.c
 *
 * Created: 4/7/2021 20:59:48
 * Author : Di
 */ 
#define F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


#define _bv(x) (0x01 << x)
#define boolean uint8_t
#define true 1
#define false 0
#define analogRead(x) read_adc(x)

//  Variables
int pulsePin = 0;
int blinkPin = 13;
int fadePin = 8;
int fadeRate = 0;




volatile int BPM;
volatile int Signal;
volatile int IBI = 600;
volatile boolean  Pulse = false;
volatile boolean  QS = false;

/
static boolean  serialVisual = true;

volatile int rate[10];
volatile unsigned long sampleCounter = 0;
volatile unsigned long lastBeatTime = 0;
volatile int P = 512;
volatile int T = 512;
volatile int thresh = 525;
volatile int amp = 100;
volatile boolean  firstBeat = true;
volatile boolean  secondBeat = false;


void arduinoSerialMonitorVisual(char symbol, int data );
void sendDataToSerial(char symbol, int data );


void usart_init( uint32_t baud)
{
	uint32_t temp;

	temp = F_CPU / 16 /baud - 1;
	
	UBRR0H = (uint8_t)(temp >> 8);
	UBRR0L = (uint8_t)temp;
	
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);

	UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}


void usart_send_byte( uint8_t data )
{
	
	while ( !( UCSR0A & (1<<UDRE0)) )
	;
	
	UDR0 = data;
}


void usart_send_str(char *dp)
{
	while(*dp != '\0')
	{
		usart_send_byte(*dp);
		dp ++;
	}
	usart_send_byte('\r');
	usart_send_byte('\n');
}


void usart_send_data_str(uint16_t num)
{
	uint8_t temp;

	temp = num / 10000 % 10;
	usart_send_byte(temp);

	temp = num / 1000 % 10;
	usart_send_byte(temp);

	temp = num / 100 % 10;
	usart_send_byte(temp);

	temp = num / 10 % 10;
	usart_send_byte(temp);

	temp = num / 1 % 10;
	usart_send_byte(temp);
}


void init_adc(void)
{
	
	ADMUX |= (1 << REFS0);
	
	ADCSRA = (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2) | (1 << ADEN);
}


uint16_t read_adc(uint8_t channel)
{
	
	ADMUX &= (0xF0);
	ADMUX |= (channel & 0x0F); /


	ADCSRA |= (1 << ADSC);

	
	while(!(ADCSRA & (1 << ADIF)));

	ADCSRA |= (1 << ADIF);

	return ADC;
}


uint16_t s_map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void interruptSetup(void)
{

	TCCR2A = 0x02;
	TCCR2B = 0x06;
	OCR2A = 0X7C;
	TIMSK2 = 0x02;
	sei();
}

void setup(void)
{

	DDRB |= _bv(5);

	
	DDRB |= _bv(0);


	usart_init(115200);


	DDRC &= _bv(0);
	PORTC &= _bv(0);
	
	interruptSetup();
	
}



void serialOutput()
{
	if (serialVisual == true)
	{
		arduinoSerialMonitorVisual('-', Signal);
	}
	else
	{
		sendDataToSerial('S', Signal);
	}
}

void serialOutputWhenBeatHappens()
{
	if (serialVisual == true)
	{
		usart_send_str("*** Heart-Beat Happened *** ");
		usart_send_str("BPM: ");
		usart_send_data_str(BPM);


	}
	else
	{
		sendDataToSerial('B',BPM);
		sendDataToSerial('Q',IBI);
	}
}


void arduinoSerialMonitorVisual(char symbol, int data )
{
	const int sensorMin = 0;
	const int sensorMax = 1024;
	int sensorReading = data;
	int range = s_map(sensorReading, sensorMin, sensorMax, 0, 11);
	switch (range)
	{
		case 0:
		usart_send_str("");
		break;
		case 1:
		usart_send_str("---");
		break;
		case 2:
		usart_send_str("------");
		break;
		case 3:
		usart_send_str("---------");
		break;
		case 4:
		usart_send_str("------------");
		break;
		case 5:
		usart_send_str("--------------|-");
		break;
		case 6:
		usart_send_str("--------------|---");
		break;
		case 7:
		usart_send_str("--------------|-------");
		break;
		case 8:
		usart_send_str("--------------|----------");
		break;
		case 9:
		usart_send_str("--------------|----------------");
		break;
		case 10:
		usart_send_str("--------------|-------------------");
		break;
		case 11:
		usart_send_str("--------------|-----------------------");
		break;
	}
}


void sendDataToSerial(char symbol, int data )
{
	usart_send_byte(symbol);
	usart_send_data_str(data);
}


ISR(TIMER2_COMPA_vect) =
{
	cli();
	Signal = analogRead(pulsePin);
	sampleCounter += 2;
	int N = sampleCounter - lastBeatTime;

	if(Signal < thresh && N > (IBI/5)*3)
	{
		if (Signal < T)
		{
			T = Signal;
		}
	}

	if(Signal > thresh && Signal > P)
	{
		P = Signal;
	}


	if (N > 250)
	{
		if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) )
		{
			Pulse = true;
	
			PORTB |= _bv(5);
			IBI = sampleCounter - lastBeatTime;
			lastBeatTime = sampleCounter;
			
			if(secondBeat)
			{
				secondBeat = false;
				for(int i=0; i<=9; i++)
				{
					rate[i] = IBI;
				}
			}
			
			if(firstBeat)
			{
				firstBeat = false;
				secondBeat = true;
				sei();
				return;
			}
	
			uint32_t runningTotal = 0;

			for(int i=0; i<=8; i++)
			{
				rate[i] = rate[i+1];
				runningTotal += rate[i];
			}

			rate[9] = IBI;
			runningTotal += rate[9];
			runningTotal /= 10;
			BPM = 60000/runningTotal;
			QS = true;
		
		}
	}

	if (Signal < thresh && Pulse == true)
	{
		PORTB &= ~_bv(5);
		Pulse = false;
		amp = P - T;
		thresh = amp/2 + T;
		P = thresh;
		T = thresh;
	}

	if (N > 2500)
	{
		thresh = 512;
		P = 512;
		T = 512;
		lastBeatTime = sampleCounter;
		firstBeat = true;
		secondBeat = false;
	}

	sei();
}



int main(void)
{

	setup();
	init_adc();
	interruptSetup();

	
    while (1) 
    {
		serialOutput();
	
		if (QS == true)
		{

			fadeRate = 255;
			serialOutputWhenBeatHappens(); /
			QS = false;
		}
	
		ledFadeToBeat();
		_delay_ms(20);
    }
}
