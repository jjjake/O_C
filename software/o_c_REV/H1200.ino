
#include "tonnetz_state.h"
#include "util_settings.h"

extern uint16_t semitones[RANGE+1];

enum EOutputMode {
  OUTPUT_CHORD_VOICING,
  OUTPUT_TUNE,
  OUTPUT_MODE_LAST
};

enum ETransformPriority {
  TRANSFORM_PRIO_XPLR,
  TRANSFORM_PRIO_XRPL,
  TRANSFORM_PRIO_LAST
};

enum EH1200Settings {
  H1200_SETTING_ROOT_OFFSET,
  H1200_SETTING_MODE,
  H1200_SETTING_INVERSION,
  H1200_SETTING_TRANFORM_PRIO,
  H1200_SETTING_OUTPUT_MODE,
  H1200_SETTING_LAST
};

static const int MAX_INVERSION = 9;

class H1200Settings : public settings::SettingsBase<H1200Settings, H1200_SETTING_LAST> {
public:

  int root_offset() const {
    return values_[H1200_SETTING_ROOT_OFFSET];
  }

  EMode mode() const {
    return static_cast<EMode>(values_[H1200_SETTING_MODE]);
  }

  int inversion() const {
    return values_[H1200_SETTING_INVERSION];
  }

  ETransformPriority get_transform_priority() const {
    return static_cast<ETransformPriority>(values_[H1200_SETTING_TRANFORM_PRIO]);
  }

  EOutputMode output_mode() const {
    return static_cast<EOutputMode>(values_[H1200_SETTING_OUTPUT_MODE]);
  }
};

const char * const output_mode_names[] = {
  "chord",
  "tune"
};

const char * const trigger_mode_names[] = {
  "P>L>R",
  "R>P>L",
};

const char * const mode_names[] = {
  "maj", "min"
};

/*static*/ template<> const settings::value_attr settings::SettingsBase<H1200Settings, H1200_SETTING_LAST>::value_attr_[] = {
  {12, -24, 36, "transpose", NULL},
  {MODE_MAJOR, 0, MODE_LAST-1, "mode", mode_names},
  {0, -3, 3, "inversion", NULL},
  {TRANSFORM_PRIO_XPLR, 0, TRANSFORM_PRIO_LAST-1, "trigger prio", trigger_mode_names},
  {OUTPUT_CHORD_VOICING, 0, OUTPUT_MODE_LAST-1, "output", output_mode_names}
};

struct H1200State {

  void init() {
    cursor_pos = 0;
    value_changed = false;
    display_notes = true;
    last_draw_millis = 0;
  
    tonnetz_state.init();
  }

  int cursor_pos;
  bool value_changed;
  bool display_notes;

  uint32_t last_draw_millis;

  TonnetzState tonnetz_state;
};

H1200Settings h1200_settings;
H1200State h1200_state;

#define H1200_OUTPUT_NOTE(i,dac) \
do { \
  int note = h1200_state.tonnetz_state.outputs(i); \
  while (note > RANGE) note -= 12; \
  while (note < 0) note += 12; \
  const uint16_t dac_code = semitones[note]; \
  DAC::set<dac>(dac_code); \
} while (0)

static const uint32_t TRIGGER_MASK_TR1 = 0x1;
static const uint32_t TRIGGER_MASK_P = 0x2;
static const uint32_t TRIGGER_MASK_L = 0x4;
static const uint32_t TRIGGER_MASK_R = 0x8;
static const uint32_t TRIGGER_MASK_DIRTY = 0x10;
static const uint32_t TRIGGER_MASK_RESET = TRIGGER_MASK_TR1 | TRIGGER_MASK_DIRTY;

void FASTRUN H1200_clock(uint32_t triggers) {

  // Since there can be simultaneous triggers, there is a definable priority.
  // Reset always has top priority
  //
  // Note: Proof-of-concept code, do not copy/paste all combinations ;)
  if (triggers & TRIGGER_MASK_RESET)
    h1200_state.tonnetz_state.reset(h1200_settings.mode());

  switch (h1200_settings.get_transform_priority()) {
    case TRANSFORM_PRIO_XPLR:
      if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
      if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
      if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
      break;

    case TRANSFORM_PRIO_XRPL:
      if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
      if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
      if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
      break;

    default: break;
  }

  int32_t sample = cvval[0];
  int root;
  if (sample < 0)
    root = 0;
  else if (sample < S_RANGE)
    root = sample >> 5;
  else
    root = RANGE;
  root += h1200_settings.root_offset();
  
  int inversion = h1200_settings.inversion() + cvval[3]; // => octave in original
  if (inversion > MAX_INVERSION) inversion = MAX_INVERSION;
  else if (inversion < -MAX_INVERSION) inversion = -MAX_INVERSION;
  h1200_state.tonnetz_state.render(root, inversion);

  switch (h1200_settings.output_mode()) {
    case OUTPUT_CHORD_VOICING: {
      H1200_OUTPUT_NOTE(0,DAC_CHANNEL_A);
      H1200_OUTPUT_NOTE(1,DAC_CHANNEL_B);
      H1200_OUTPUT_NOTE(2,DAC_CHANNEL_C);
      H1200_OUTPUT_NOTE(3,DAC_CHANNEL_D);
    }
    break;
    case OUTPUT_TUNE: {
      H1200_OUTPUT_NOTE(0,DAC_CHANNEL_A);
      H1200_OUTPUT_NOTE(0,DAC_CHANNEL_B);
      H1200_OUTPUT_NOTE(0,DAC_CHANNEL_C);
      H1200_OUTPUT_NOTE(0,DAC_CHANNEL_D);
    }
    break;
    default: break;
  }

  if (triggers || millis() - h1200_state.last_draw_millis > 1000) {
    MENU_REDRAW = 1;
    h1200_state.last_draw_millis = millis();
  }
}

void H1200_init() {
  h1200_settings.init_defaults();
  h1200_state.init();
  init_circle_lut();
}

static const size_t H1200_SETTINGS_SIZE = sizeof(int8_t) * H1200_SETTING_LAST;

size_t H1200_save(char *storage) {
  return h1200_settings.save<int8_t>(storage);
}

size_t H1200_restore(const char *storage) {
  return h1200_settings.restore<int8_t>(storage);
}

void H1200_resume() {
  encoder[LEFT].setPos(h1200_state.cursor_pos);
  encoder[RIGHT].setPos(h1200_settings.get_value(h1200_state.cursor_pos));
  h1200_state.tonnetz_state.reset(h1200_settings.mode());
}

#define CLOCKIT() \
do { \
  uint32_t triggers = 0; \
  if (CLK_STATE[TR1]) { triggers |= TRIGGER_MASK_TR1; CLK_STATE[TR1] = false; } \
  if (CLK_STATE[TR2]) { triggers |= TRIGGER_MASK_P; CLK_STATE[TR2] = false; } \
  if (CLK_STATE[TR3]) { triggers |= TRIGGER_MASK_L; CLK_STATE[TR3] = false; } \
  if (CLK_STATE[TR4]) { triggers |= TRIGGER_MASK_R; CLK_STATE[TR4] = false; } \
  if (h1200_state.value_changed) { triggers |= TRIGGER_MASK_DIRTY; h1200_state.value_changed = false; } \
  H1200_clock(triggers); \
} while (0)

void H1200_loop() {
  CLOCKIT();
  UI();
  CLOCKIT();
  if (_ADC) CV();
  CLOCKIT();
  if (_ENC && (millis() - _BUTTONS_TIMESTAMP > DEBOUNCE)) encoders();
  CLOCKIT();
  buttons(BUTTON_BOTTOM);
  CLOCKIT();
  buttons(BUTTON_TOP);
  CLOCKIT();
  buttons(BUTTON_LEFT);
  CLOCKIT();
  buttons(BUTTON_RIGHT);
  CLOCKIT();
}