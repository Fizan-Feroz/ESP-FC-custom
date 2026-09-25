#include <FastLED.h>

#define LED_PIN 16
#define NUM_LEDS 28
#define LEG_LEDS 7

CRGB leds[NUM_LEDS];

void setup()
{
  Serial.begin(115200);
  delay(500);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(80);
  FastLED.clear();
  FastLED.show();
}

void loop()
{
  const uint32_t STEP_MS = 250;
  const uint32_t GAP_MS = 400;
  static uint32_t last = 0;
  static int leg = 0;
  static int idx = 0;

  uint32_t now = millis();
  uint32_t delayMs = (idx < LEG_LEDS) ? STEP_MS : GAP_MS;
  if (now - last < delayMs) return;
  last = now;

  FastLED.clear();
  if (idx < LEG_LEDS)
  {
    int i = leg * LEG_LEDS + idx;
    leds[i] = CRGB::White;
    Serial.printf("leg %d  index %d (data %d)\n", leg, idx, i);
  }
  else
  {
    Serial.printf("--- gap, next leg\n");
  }
  FastLED.show();

  idx++;
  if (idx > LEG_LEDS)
  {
    idx = 0;
    leg = (leg + 1) % 4;
  }
}
