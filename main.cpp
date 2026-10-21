/*
 * 駐車アシストアラームのC++11学習・リファクタリング版。
 * センサー計測、発音、警告判断を別のクラスにまとめる。
 * 元のmain.cは残し、計測式・警告条件・ソフトウェアループ方式を維持する。
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// センサーに関係する設定と処理をまとめ、他のクラスからは計測だけを呼ぶ。
class UltrasonicSensor {
public:
    // publicはクラスの外から使える入口。コンストラクタは生成時に一度呼ばれる。
    UltrasonicSensor()
    {
        // DDRAのビットを1にすると出力、0にすると入力になる。
        // |= と &= で対象ビットだけを変更し、他の端子の設定を保つ。
        DDRA |= (1 << kTrigPin);
        DDRA &= ~(1 << kEchoPin);
    }

    long measureDistanceCm()
    {
        // PORTAは出力値、PINAは入力端子の状態を扱うレジスタ。
        PORTA &= ~(1 << kTrigPin);
        _delay_us(2);
        PORTA |= (1 << kTrigPin);
        _delay_us(10);
        PORTA &= ~(1 << kTrigPin);

        unsigned long timeoutCounter = kMeasureTimeout;
        while (!(PINA & (1 << kEchoPin))) {
            if (timeoutCounter == 0) {
                break;
            }
            timeoutCounter--;
        }

        unsigned long duration = 0;
        if (timeoutCounter > 0) {
            timeoutCounter = kMeasureTimeout;
            while (PINA & (1 << kEchoPin)) {
                if (timeoutCounter == 0) {
                    break;
                }
                duration++;
                _delay_us(1);
                timeoutCounter--;
            }
        }

        if (timeoutCounter == 0) {
            duration = 0;
        }

        // 元の計算式と型を維持する。durationは厳密な経過時間ではなく反復回数。
        // longへの代入で小数部分が切り捨てられ、タイムアウト時も0cmを返す。
        long distance = duration * 0.034 / 2;
        return distance;
    }

private:
    // privateはクラス内部でのみ使う実装。外からピン設定を書き換えさせない。
    // constexprはコンパイル時に決まる定数。staticなので個体ごとの設定値を持たない。
    static constexpr uint8_t kTrigPin = PA0;
    static constexpr uint8_t kEchoPin = PA1;
    static constexpr unsigned long kMeasureTimeout = 40000UL;
};

// 圧電サウンダのGPIO操作と、矩形波による発音を担当する。
class Buzzer {
public:
    Buzzer()
    {
        DDRA |= (1 << kPiezoPin);
    }

    void beep(unsigned int frequency, unsigned int durationMs)
    {
        if (frequency == 0) {
            return;
        }

        // 元のC版と同じ整数計算。周波数・時間は設定値であり実測値ではない。
        long halfPeriodUs = 500000L / frequency;
        // C版の(long)と同じ型変換。掛け算の前にlongにして16bitでの積を避ける。
        long cycles = static_cast<long>(durationMs) * 1000 / (halfPeriodUs * 2);

        for (long i = 0; i < cycles; i++) {
            PORTA |= (1 << kPiezoPin);
            variableDelayUs(halfPeriodUs);
            PORTA &= ~(1 << kPiezoPin);
            variableDelayUs(halfPeriodUs);
        }
    }

private:
    static constexpr uint8_t kPiezoPin = PA2;

    // static関数はオブジェクト固有の状態を使わない処理に用いる。
    // _delay_us()自体の引数を定数にし、可変回数だけ繰り返す元の方法を残す。
    static void variableDelayUs(unsigned int microseconds)
    {
        for (unsigned int i = 0; i < microseconds; i++) {
            _delay_us(1);
        }
    }
};

// センサーとサウンダをメンバとして持ち、距離に応じた警告を組み合わせる。
// 継承やvirtual関数は使わず、「必要な部品を持つ」という関係で構成する。
class ParkingAssist {
public:
    // 初期化リストでメンバを生成する。実際の生成順はprivateでの宣言順。
    // sensor_、buzzer_、この本体の順なので、GPIO設定順もC版と同じになる。
    ParkingAssist() : sensor_(), buzzer_()
    {
        DDRA |= (1 << kLedPin);
    }

    void run()
    {
        while (1) {
            long distance = sensor_.measureDistanceCm();
            updateAlarm(distance);
        }
    }

private:
    // 状態を持たない小さな部品を直接メンバにする。newや動的な配列は使わない。
    UltrasonicSensor sensor_;
    Buzzer buzzer_;

    static constexpr uint8_t kLedPin = PA3;
    static constexpr unsigned int kDangerFrequencyHz = 1500;
    static constexpr unsigned int kCloseFrequencyHz = 1200;
    static constexpr unsigned int kMidFrequencyHz = 1000;
    static constexpr unsigned int kDangerDurationMs = 50;
    static constexpr unsigned int kCloseDurationMs = 75;
    static constexpr unsigned int kMidDurationMs = 100;
    static constexpr unsigned int kCloseIntervalMs = 75;
    static constexpr unsigned int kMidIntervalMs = 300;

    void updateAlarm(long distance)
    {
        // 条件とGPIO操作の順序はC版のまま。発音・待機中は再測定しない。
        if (distance > 0 && distance < 5) {
            PORTA |= (1 << kLedPin);
            buzzer_.beep(kDangerFrequencyHz, kDangerDurationMs);
        } else if (distance >= 5 && distance < 15) {
            PORTA |= (1 << kLedPin);
            buzzer_.beep(kCloseFrequencyHz, kCloseDurationMs);
            PORTA &= ~(1 << kLedPin);
            _delay_ms(kCloseIntervalMs);
        } else if (distance >= 15 && distance < 30) {
            PORTA |= (1 << kLedPin);
            buzzer_.beep(kMidFrequencyHz, kMidDurationMs);
            PORTA &= ~(1 << kLedPin);
            _delay_ms(kMidIntervalMs);
        } else {
            // 30cm以上に加え、整数0cm・測定タイムアウト時も元の仕様では停止する。
            PORTA &= ~(1 << kLedPin);
        }
    }
};

int main()
{
    // C版のmainに集まっていた初期設定と警告ループをオブジェクトに任せる。
    // main内で生成するので、コンストラクタのGPIO操作はmain開始後に実行される。
    ParkingAssist parkingAssist;
    parkingAssist.run();
}
