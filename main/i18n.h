#pragma once

#include "settings.h"

// Translated texts. tr() returns the text in the language chosen in the
// settings. Texts that contain %d are printf formats.
namespace i18n {

#define I18N_STRINGS(X) \
    /* id,           English,                    French,                          German,                       Spanish,                       Italian */ \
    X(HEART_RATE,    "HEART RATE",               "FREQ. CARDIAQUE",               "HERZFREQUENZ",               "FREC. CARDIACA",              "FREQ. CARDIACA") \
    X(NO_STRAP,      "NO STRAP",                 "PAS DE CEINTURE",               "KEIN GURT",                  "SIN BANDA",                   "NESSUNA FASCIA") \
    X(WAIT_PULSE,    "WAITING FOR PULSE",        "ATTENTE DU POULS",              "WARTE AUF PULS",             "ESPERANDO PULSO",             "ATTESA POLSO") \
    X(SPEED,         "SPEED",                    "VITESSE",                       "TEMPO",                      "VELOC.",                      "VELOC.") \
    X(BEST,          "BEST",                     "MEILL.",                        "BESTE",                      "MEJOR",                       "MIGL.") \
    X(GAP,           "GAP",                      "ÉCART",                         "DIFF.",                      "DIF.",                        "DIFF.") \
    X(LEAN,          "LEAN",                     "ANGLE",                         "WINKEL",                     "ÁNGULO",                      "ANGOLO") \
    X(LEFT_SHORT,    "L",                        "G",                             "L",                          "I",                           "S") \
    X(RIGHT_SHORT,   "R",                        "D",                             "R",                          "D",                           "D") \
    X(CURRENT,       "CURRENT",                  "TOUR ACTUEL",                   "AKTUELL",                    "ACTUAL",                      "ATTUALE") \
    X(CURRENT_LAP,   "CURRENT  LAP %d",          "TOUR %d EN COURS",              "AKTUELLE RUNDE %d",          "VUELTA ACTUAL %d",            "GIRO ATTUALE %d") \
    X(NO_LINE,       "NO LINE - PRESS MARK",     "PAS DE LIGNE: MARK",            "KEINE LINIE: MARK",          "SIN META: PULSE MARK",        "NO TRAGUARDO: MARK") \
    X(WAIT_LINE,     "WAITING FOR LINE...",      "ATTENTE DE LA LIGNE",           "WARTE AUF LINIE...",         "ESPERANDO META...",           "ATTESA TRAGUARDO...") \
    X(LAP,           "LAP",                      "TOUR",                          "RUNDE",                      "VUELTA",                      "GIRO") \
    X(NO_FIX,        "NO FIX",                   "PAS DE FIX",                    "KEIN FIX",                   "SIN FIX",                     "NO FIX") \
    X(SAT,           "sat",                      "sat",                           "Sat",                        "sat",                         "sat") \
    X(CHIP_HEART,    "Heart",                    "Pouls",                         "Puls",                       "Pulso",                       "Polso") \
    X(ST_SEARCH,     "SEARCH",                   "RECH.",                         "SUCHE",                      "BUSCA",                       "CERCA") \
    X(ST_LINK,       "LINK",                     "LIEN",                          "LINK",                       "ENLACE",                      "LINK") \
    X(ST_READY,      "READY",                    "PRÊT",                          "BEREIT",                     "LISTO",                       "PRONTO") \
    X(ST_REC,        "REC",                      "REC",                           "REC",                        "REC",                         "REC") \
    X(ST_PAUSED,     "PAUSED",                   "PAUSE",                         "PAUSE",                      "PAUSA",                       "PAUSA") \
    X(ST_LOCKED,     "LOCKED",                   "VERR.",                         "GESP.",                      "BLOQ.",                       "BLOCC.") \
    X(ST_PAIRING,    "PAIRING",                  "ASSOC.",                        "KOPPELN",                    "VINCUL.",                     "ASSOC.") \
    X(ST_LIVE,       "LIVE",                     "ACTIF",                         "LIVE",                       "ACTIVO",                      "ATTIVO") \
    X(MODE_VIDEO,    "Video",                    "Vidéo",                         "Video",                      "Vídeo",                       "Video") \
    X(MODE_PHOTO,    "Photo",                    "Photo",                         "Foto",                       "Foto",                        "Foto") \
    X(MODE_TIMELAPSE,"Timelapse",                "Timelapse",                     "Zeitraffer",                 "Timelapse",                   "Timelapse") \
    X(SETTINGS,      "Settings",                 "Réglages",                      "Einstellungen",              "Ajustes",                     "Impostazioni") \
    X(BACK,          "Back",                     "Retour",                        "Zurück",                     "Atrás",                       "Indietro") \
    X(LANGUAGE,      "Language",                 "Langue",                        "Sprache",                    "Idioma",                      "Lingua") \
    X(SPEED_UNIT,    "Speed unit",               "Unité de vitesse",              "Geschwindigkeitseinheit",    "Unidad de velocidad",         "Unità di velocità") \
    X(BRIGHTNESS,    "Brightness",               "Luminosité",                    "Helligkeit",                 "Brillo",                      "Luminosità") \
    X(DEMO_MODE,     "Demo mode",                "Mode démo",                     "Demo-Modus",                 "Modo demo",                   "Modalità demo") \
    X(FORGET_BT,     "Forget Bluetooth devices", "Oublier les appareils Bluetooth", "Bluetooth-Geräte vergessen", "Olvidar dispositivos Bluetooth", "Dimentica i dispositivi Bluetooth") \
    X(RESET_LAPS,    "Reset laps and max lean",  "Réinitialiser tours et angle max", "Runden und Max-Winkel löschen", "Reiniciar vueltas y ángulo máx.", "Azzera giri e angolo max") \
    X(RESTART,       "Restart",                  "Redémarrer",                    "Neustart",                   "Reiniciar",                   "Riavvia") \
    X(FIRMWARE,      "Firmware",                 "Firmware",                      "Firmware",                   "Firmware",                    "Firmware") \
    X(SYSTEM,        "System",                   "Système",                       "System",                     "Sistema",                     "Sistema") \
    X(SYS_RAM,       "RAM free",                 "RAM libre",                     "RAM frei",                   "RAM libre",                   "RAM libera") \
    X(SYS_PSRAM,     "PSRAM free",               "PSRAM libre",                   "PSRAM frei",                 "PSRAM libre",                 "PSRAM libera") \
    X(SYS_UPTIME,    "Uptime",                   "En marche",                     "Laufzeit",                   "Activo",                      "In funzione") \
    X(DONE_BT,      "Bluetooth devices forgotten", "Appareils Bluetooth oubliés", "Bluetooth-Geräte vergessen", "Dispositivos Bluetooth olvidados", "Dispositivi Bluetooth dimenticati") \
    X(DONE_LAPS,     "Laps reset",               "Tours réinitialisés",           "Runden gelöscht",            "Vueltas reiniciadas",         "Giri azzerati")

enum Str : uint8_t {
#define X(id, en, fr, de, es, it) id,
    I18N_STRINGS(X)
#undef X
    STR_COUNT
};

const char* tr(Str id);                 // text in the current language
const char* language_name(uint8_t lang); // each language written in itself

} // namespace i18n
