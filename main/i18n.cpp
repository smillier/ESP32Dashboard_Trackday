#include "i18n.h"

namespace i18n {
namespace {

const char* const kTable[STR_COUNT][settings::LANG_COUNT] = {
#define X(id, en, fr, de, es, it) {en, fr, de, es, it},
    I18N_STRINGS(X)
#undef X
};

const char* const kLanguageNames[settings::LANG_COUNT] = {"English", "Français", "Deutsch", "Español", "Italiano"};

} // namespace

const char* tr(Str id) {
    if (id >= STR_COUNT) return "";
    return kTable[id][settings::get().lang];
}

const char* language_name(uint8_t lang) {
    return lang < settings::LANG_COUNT ? kLanguageNames[lang] : "";
}

} // namespace i18n
