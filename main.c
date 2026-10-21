/*
 * 駐車アシストアラーム (ATtiny84A / AVR-GCC)
 * 可変の遅延時間は _delay_us(1) を繰り返して扱う。
 */

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

// =================================================================
// 1. ピンの定義 (ATtiny84A用)
// =================================================================
#define TRIG_DDR DDRA
#define TRIG_PORT PORTA
#define TRIG_PIN_BIT PA0

#define ECHO_DDR DDRA
#define ECHO_PORT PINA
#define ECHO_PIN_BIT PA1

#define PIEZO_DDR DDRA
#define PIEZO_PORT PORTA
#define PIEZO_PIN_BIT PA2

#define LED_DDR DDRA
#define LED_PORT PORTA
#define LED_PIN_BIT PA3

// =================================================================
// 2. 調整用の設定値
// =================================================================
const unsigned int FREQ_DANGER = 1500;
const unsigned int FREQ_CLOSE = 1200;
const unsigned int FREQ_MID = 1000;

const unsigned int BEEP_DURATION_CLOSE = 75;
const unsigned int BEEP_DURATION_MID = 100;

const unsigned int BEEP_INTERVAL_CLOSE = 75;
const unsigned int BEEP_INTERVAL_MID = 300;

const unsigned long MEASURE_TIMEOUT = 40000;

// =================================================================
// 3. 自作の関数
// =================================================================

// 変数で指定した回数だけ _delay_us(1) を繰り返す関数
void variable_delay_us(unsigned int microseconds)
{
    for (unsigned int i = 0; i < microseconds; i++) {
        _delay_us(1);
    }
}

// 指定された周波数と長さで、ピエゾを鳴らす関数
void beep(unsigned int frequency, unsigned int duration_ms)
{
    if (frequency == 0) {
        return;
    }

    long half_period_us = 500000L / frequency;
    long cycles = (long)duration_ms * 1000 / (half_period_us * 2);

    for (long i = 0; i < cycles; i++) {
        PIEZO_PORT |= (1 << PIEZO_PIN_BIT);
        variable_delay_us(half_period_us);
        PIEZO_PORT &= ~(1 << PIEZO_PIN_BIT);
        variable_delay_us(half_period_us);
    }
}

// =================================================================
// 4. main()関数：プログラムの本体
// =================================================================
int main(void)
{
    // 初期設定
    TRIG_DDR |= (1 << TRIG_PIN_BIT);
    ECHO_DDR &= ~(1 << ECHO_PIN_BIT);
    PIEZO_DDR |= (1 << PIEZO_PIN_BIT);
    LED_DDR |= (1 << LED_PIN_BIT);

    // 無限ループ
    while (1) {
        // ステップ1: 距離を測定する
        TRIG_PORT &= ~(1 << TRIG_PIN_BIT);
        _delay_us(2);
        TRIG_PORT |= (1 << TRIG_PIN_BIT);
        _delay_us(10);
        TRIG_PORT &= ~(1 << TRIG_PIN_BIT);

        unsigned long timeout_counter = MEASURE_TIMEOUT;
        while (!(ECHO_PORT & (1 << ECHO_PIN_BIT))) {
            if (timeout_counter == 0) {
                break;
            }
            timeout_counter--;
        }

        unsigned long duration = 0;
        if (timeout_counter > 0) {
            timeout_counter = MEASURE_TIMEOUT;
            while (ECHO_PORT & (1 << ECHO_PIN_BIT)) {
                if (timeout_counter == 0) {
                    break;
                }
                duration++;
                _delay_us(1);
                timeout_counter--;
            }
        }

        if (timeout_counter == 0) {
            duration = 0;
        }

        long distance = duration * 0.034 / 2;

        // ステップ2: 距離に応じて警告レベルを判断し、音とLEDを制御
        if (distance > 0 && distance < 5) {
            LED_PORT |= (1 << LED_PIN_BIT);
            beep(FREQ_DANGER, 50);
        } else if (distance >= 5 && distance < 15) {
            LED_PORT |= (1 << LED_PIN_BIT);
            beep(FREQ_CLOSE, BEEP_DURATION_CLOSE);
            LED_PORT &= ~(1 << LED_PIN_BIT);
            _delay_ms(BEEP_INTERVAL_CLOSE);
        } else if (distance >= 15 && distance < 30) {
            LED_PORT |= (1 << LED_PIN_BIT);
            beep(FREQ_MID, BEEP_DURATION_MID);
            LED_PORT &= ~(1 << LED_PIN_BIT);
            _delay_ms(BEEP_INTERVAL_MID);
        } else {
            LED_PORT &= ~(1 << LED_PIN_BIT);
        }
    }
}
