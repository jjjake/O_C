#include "streams_lorenz_generator.h"
#include "util_math.h"
#include "OC_digital_inputs.h"

enum LORENZ_SETTINGS {
  LORENZ_SETTING_FREQ1,
  LORENZ_SETTING_FREQ2,
  LORENZ_SETTING_SIGMA,
  LORENZ_SETTING_RHO,
  LORENZ_SETTING_BETA,
  LORENZ_SETTING_LAST
};

class LorenzGenerator : public settings::SettingsBase<LorenzGenerator, LORENZ_SETTING_LAST> {
public:

  uint16_t get_freq1() const {
    return values_[LORENZ_SETTING_FREQ1];
  }

  uint16_t get_freq2() const {
    return values_[LORENZ_SETTING_FREQ2];
  }

  uint16_t get_sigma() const {
    return values_[LORENZ_SETTING_SIGMA];
  }

  uint16_t get_rho() const {
    return values_[LORENZ_SETTING_RHO];
  }

  uint16_t get_beta() const {
    return values_[LORENZ_SETTING_BETA];
  }

  void Init();

  void freeze() {
    frozen_ = true;
  }

  void thaw() {
    frozen_ = false;
  }

  bool frozen() const {
    return frozen_;
  }

  streams::LorenzGenerator lorenz;
  bool frozen_;

  // ISR update is at 16.666kHz, we don't need it that fast so smooth the values to ~1Khz
  static constexpr int32_t kSmoothing = 16;

  SmoothedValue<int32_t, kSmoothing> cv_freq1;
  SmoothedValue<int32_t, kSmoothing> cv_freq2;
  SmoothedValue<int32_t, kSmoothing> cv_sigma;
  SmoothedValue<int32_t, kSmoothing> cv_rho;
};

void LorenzGenerator::Init() {
  InitDefaults();
  lorenz.Init();
  frozen_= false;
}

/*static*/ template <>
const settings::value_attr settings::SettingsBase<LorenzGenerator, LORENZ_SETTING_LAST>::value_attr_[] = {
  { 0, 0, 255, "FREQ 1", NULL },
  { 0, 0, 255, "FREQ 2", NULL },
  { 10, 7, 20, "SIGMA", NULL }, // 10 is sweet spot
  { 28, 24, 35, "RHO", NULL }, // 28 is sweet spot
  { 8, 4, 11, "BETA", NULL }, // 8 (/3) is sweet spot
};

LorenzGenerator lorenz_generator;
struct {
  int selected_param;
  bool selected_generator;
} lorenz_generator_state;


#define SCALE8_16(x) ((((x + 1) << 16) >> 8) - 1)

void FASTRUN LORENZ_isr() {

  bool reset_phase1 = OC::DigitalInputs::clocked<TR1>();
  bool freeze = OC::DigitalInputs::read_immediate<TR2>();
  bool reset_phase2 = OC::DigitalInputs::clocked<TR3>();
  bool freeze2 = OC::DigitalInputs::read_immediate<TR4>();

  lorenz_generator.cv_freq1.push(OC::ADC::value<ADC_CHANNEL_1>());
  lorenz_generator.cv_freq2.push(OC::ADC::value<ADC_CHANNEL_2>());
  lorenz_generator.cv_sigma.push(OC::ADC::value<ADC_CHANNEL_3>());
  lorenz_generator.cv_rho.push(OC::ADC::value<ADC_CHANNEL_4>());

  // Range in settings is (0-256] so this gets scaled to (0,65535]
  // CV value is 12 bit so also needs scaling

  int32_t freq1 = SCALE8_16(lorenz_generator.get_freq1()) + (lorenz_generator.cv_freq1.value() * 16);
  freq1 = USAT16(freq1);

  int32_t freq2 = SCALE8_16(lorenz_generator.get_freq2()) + (lorenz_generator.cv_freq2.value() * 16);
  freq2 = USAT16(freq2);

  // int32_t sigma = lorenz_generator.get_sigma() + (lorenz_generator.cv_sigma.value() << 7);
  int32_t sigma = lorenz_generator.get_sigma() ;
  lorenz_generator.lorenz.set_sigma(USAT16(sigma));

  // int32_t rho = lorenz_generator.get_rho() + (lorenz_generator.cv_rho.value() << 6);
  int32_t rho = lorenz_generator.get_rho() ;
  lorenz_generator.lorenz.set_rho(USAT16(rho));

  // int32_t beta = lorenz_generator.get_beta() + (lorenz_generator.cv_beta.value() << 6);
  int32_t beta = lorenz_generator.get_beta() ;
  lorenz_generator.lorenz.set_beta(USAT16(beta));

  if (!freeze && !lorenz_generator.frozen())
    lorenz_generator.lorenz.Process(freq1, freq2, reset_phase1, reset_phase2);

  DAC::set<DAC_CHANNEL_A>(lorenz_generator.lorenz.dac_code(0));
  DAC::set<DAC_CHANNEL_B>(lorenz_generator.lorenz.dac_code(1));
  DAC::set<DAC_CHANNEL_C>(lorenz_generator.lorenz.dac_code(2));
  DAC::set<DAC_CHANNEL_D>(lorenz_generator.lorenz.dac_code(3));
}

void LORENZ_init() {
  lorenz_generator_state.selected_param = LORENZ_SETTING_SIGMA;
  lorenz_generator.Init();
}

static const size_t LORENZ_SETTINGS_SIZE = sizeof(uint8_t) * LORENZ_SETTING_LAST;

size_t LORENZ_save(char *storage) {
  return lorenz_generator.save<uint8_t>(storage);
}

size_t LORENZ_restore(const char *storage) {
  return lorenz_generator.restore<uint8_t>(storage);
}

void LORENZ_loop() {
  UI();
  if (_ENC && (millis() - _BUTTONS_TIMESTAMP > DEBOUNCE)) encoders();
  buttons(BUTTON_TOP);
  buttons(BUTTON_BOTTOM);
  buttons(BUTTON_LEFT);
  buttons(BUTTON_RIGHT);
}

void LORENZ_menu() {
  GRAPHICS_BEGIN_FRAME(false); // no frame, no problem

  graphics.setFont(UI_DEFAULT_FONT);

  static const weegfx::coord_t kStartX = 0;
  UI_DRAW_TITLE(kStartX);
  graphics.setPrintPos(2, 2);
  graphics.print("R1=");
  graphics.print(lorenz_generator.get_value(LORENZ_SETTING_FREQ1));
  graphics.print("+");
  graphics.print(lorenz_generator.cv_freq1.value() >> 4);
  graphics.setPrintPos(66, 2);
  graphics.print("R2=");
  graphics.print(lorenz_generator.get_value(LORENZ_SETTING_FREQ2));
  graphics.print("+");
  graphics.print(lorenz_generator.cv_freq2.value() >> 4);
  if (lorenz_generator_state.selected_generator) {
      graphics.invertRect(66, 0, 127, 10);
  } else {
      graphics.invertRect(2, 0, 64, 10);    
  }
  

  int first_visible_param = LORENZ_SETTING_SIGMA; 

  UI_START_MENU(kStartX);
  UI_BEGIN_ITEMS_LOOP(kStartX, first_visible_param, LORENZ_SETTING_LAST, lorenz_generator_state.selected_param, 0)
    const settings::value_attr &attr = LorenzGenerator::value_attr(current_item);
    UI_DRAW_SETTING(attr, lorenz_generator.get_value(current_item), kUiWideMenuCol1X);
  UI_END_ITEMS_LOOP();

  GRAPHICS_END_FRAME();
}

// static const size_t kLargePreviewBufferSize = 64;
// uint16_t large_preview_buffer[kLargePreviewBufferSize];

// weegfx::coord_t scanner = 0;
// unsigned scanner_millis = 0;
// static const unsigned SCANNER_RATE = 200;

void LORENZ_screensaver() {
  GRAPHICS_BEGIN_FRAME(false);

//  uint16_t shape = poly_lfo.get_value(POLYLFO_SETTING_SHAPE) << 8;
//  poly_lfo.lfo.RenderPreview(shape, large_preview_buffer, kLargePreviewBufferSize);
//  for (weegfx::coord_t x = 0; x < 128; ++x) {
//    graphics.setPixel(x, 32 - (large_preview_buffer[(x + scanner) & 63] >> 11));
//  }
//  if (millis() - scanner_millis > SCANNER_RATE) {
//    ++scanner;
//    scanner_millis = millis();
//  }

// lorenz_scope_render();
scope_render();

  GRAPHICS_END_FRAME();
}

void LORENZ_resume() {
  if (lorenz_generator_state.selected_generator) {
    encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ2));    
  } else {
    encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ1));
  }
  encoder[RIGHT].setPos(lorenz_generator.get_value(lorenz_generator_state.selected_param));
}

void LORENZ_topButton() {
//  lorenz_generator.change_value(LORENZ_SETTING_FREQ, 32);
//  encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ));
  lorenz_generator.lorenz.Init();
}

void LORENZ_lowerButton() {
//  lorenz_generator.change_value(LORENZ_SETTING_FREQ, -32);
//  encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ));
}

void LORENZ_rightButton() {
  ++lorenz_generator_state.selected_param;
  if (lorenz_generator_state.selected_param >= LORENZ_SETTING_LAST)
    lorenz_generator_state.selected_param = LORENZ_SETTING_SIGMA;
  encoder[RIGHT].setPos(lorenz_generator.get_value(lorenz_generator_state.selected_param));
}

void LORENZ_leftButton() {
  lorenz_generator_state.selected_generator = !lorenz_generator_state.selected_generator;
  if (lorenz_generator_state.selected_generator) {
        encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ2));
  } else {
        encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ1));
  }

}

void LORENZ_leftButtonLong() {
}

bool LORENZ_encoders() {
  bool changed = false;
  int value = encoder[LEFT].pos();
  if (lorenz_generator_state.selected_generator) {
    if (value != lorenz_generator.get_value(LORENZ_SETTING_FREQ2)) {
      lorenz_generator.apply_value(LORENZ_SETTING_FREQ2, value);
      encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ2));
      changed = true;
    }
  } else {
    if (value != lorenz_generator.get_value(LORENZ_SETTING_FREQ1)) {
      lorenz_generator.apply_value(LORENZ_SETTING_FREQ1, value);
      encoder[LEFT].setPos(lorenz_generator.get_value(LORENZ_SETTING_FREQ1));
      changed = true;
    }
  }

  value = encoder[RIGHT].pos();
  if (value != lorenz_generator.get_value(lorenz_generator_state.selected_param)) {
    lorenz_generator.apply_value(lorenz_generator_state.selected_param, value);
    encoder[RIGHT].setPos(lorenz_generator.get_value(lorenz_generator_state.selected_param));
    changed = true;
  }

  return changed;
}

void LORENZ_debug() {
  graphics.setPrintPos(2, 12);
  graphics.print(lorenz_generator.cv_freq1.value());
  graphics.print(" ");
  int32_t value = SCALE8_16(lorenz_generator.get_freq1());
  graphics.print(value);
//  graphics.print(" ");
//  graphics.print(lorenz_generator.cv_shape.value() * 16);
//  value += lorenz_generator.cv_shape.value() * 16;
//  graphics.setPrintPos(2, 22);
//  graphics.print(value); graphics.print(" ");
//  value = USAT16(value);
//  graphics.print(value);
}


