#pragma once
// DREAMCATCHER - Klangkern (reines C++, ohne JUCE)
//
// Die vier Bausteine sind Portierungen von Airwindows-Code (Chris Johnson, MIT-Lizenz, siehe LICENSES):
//   SweetWide      -> SweetWide
//   StereoEnsemble -> StereoEnsemble
//   GlitchShifter  -> GlitchShifter
//   PitchNasty     -> PitchNasty
// Angepasst wurden nur die Struktur (eine Klasse je Baustein, Abtastrate frei), die Dither-Schleife wurde
// weggelassen. Die Algorithmen selbst sind unveraendert uebernommen.
//
// Kette (je Modus mit anderen Anteilen):  Eingang -> GlitchShifter -> PitchNasty -> SweetWide
//                                         -> StereoEnsemble -> Hoehen-/Tiefenfilter -> Ausgang
// Der Fader "Effekt" blendet zwischen unbearbeiteter Stimme (0 %) und dem vollen Modus-Klang (100 %).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dch
{
enum Mode { kInner = 0, kMemory = 1, kDream = 2, kGhost = 3, kNumModes };

// ======================================================================================================
//  KLANGWERTE - hier wird der Klang der drei Modi abgestimmt (alles an einer Stelle)
// ======================================================================================================
struct Voicing
{
    double glitchWet;    // Anteil GlitchShifter (Stottern, leichter Tonhoehenabfall), 0..1
    double nastyWet;     // Anteil PitchNasty (koernige Tonhoehenverschiebung), 0..1
    double wideSoar;     // SweetWide: Staerke 0..0.3 (0 = aus)
    double wideBlend;    // SweetWide: Anteil der Verbreiterung -0.5..0.5
    double ensDepth;     // StereoEnsemble: Laenge der Echos 0..1
    double ensLevel;     // StereoEnsemble: Anteil Echos 0..1
    double highpassHz;   // Tiefen weg
    double lowpassHz;    // Hoehen weg (dumpfer = weiter weg / aelter)
    double trim;         // fester Pegelausgleich (linear), damit 100 % etwa so laut ist wie 0 %
    // Schwapp-Echo (Ping-Pong, optional anschwellend)
    double echoLevel;    // Lautstaerke der Echos 0..1 (0 = aus)
    double echoPreMs;    // Pre-Delay vor dem ersten Echo
    double echoSpaceMs;  // Abstand zwischen den Echos
    double echoSwell;    // 0 = Echos werden gleich leiser, 1 = Echos schwellen erst an und klingen dann aus
    double echoWidth;    // 0 = alle Echos mittig, 1 = Echos wandern ganz zwischen links und rechts
    double echoLpHz;     // Dunkelheit der Echos
    // Reverse-Hall (nur Geist)
    double ghostDry;     // Anteil der (verzoegerten) Stimme, die am Ende des Schwells einsetzt
    double ghostWet;     // Anteil des anschwellenden Reverse-Halls
    // Schwimmen (Airwindows Flutter / GalacticVibe) und verstimmte Echos
    double flutterWet;   // Anteil Flutter (unregelmaessiges Bandleiern), 0..1
    double vibeWet;      // Anteil GalacticVibe (langsames, zufaelliges Wabern der Tonhoehe), 0..1
    double echoDriftMs;  // Echos wandern in der Tonhoehe: Schwankung der Echo-Zeit in ms (0 = aus)
};

// Reihenfolge wie enum Mode
static const Voicing kVoicing[kNumModes] = {
    // innere Stimme: nah, im Kopf, leicht verbreitert, nur ein kurzes leises Echo dahinter, wenig dunkler
    { 0.00, 0.00, 0.12, 0.25, 0.35, 0.22, 140.0, 7500.0, 1.47,   0.18,  18.0,  90.0, 0.0, 0.3, 3500.0,   0.00, 0.00,   0.00, 0.00, 0.0 },
    // Erinnerung: stark zerfallende, dumpfe Stimme, dazu Pre-Delay und Echos, die schwappend zwischen links und rechts anschwellen
    { 0.55, 0.00, 0.20, 0.20, 0.55, 0.15, 260.0, 3800.0, 2.96,   0.28, 120.0, 240.0, 0.7, 1.0, 2600.0,   0.00, 0.00,   1.00, 0.00, 0.0 },
    // Traum: koernig verstimmt, weite Wolke, dazu langsam anschwellende Echos
    { 0.00, 0.45, 0.28, 0.40, 0.90, 0.30, 160.0, 4200.0, 4.48,   0.20,  60.0, 330.0, 1.0, 0.8, 3000.0,   0.00, 0.00,   0.00, 0.80, 5.0 },
    // Geist: der Hall laeuft rueckwaerts und schwillt vor der Stimme an (Latenz, siehe kGhostSeconds)
    { 0.00, 0.00, 0.10, 0.20, 0.25, 0.15, 150.0, 6000.0, 0.99,   0.00,   0.0,   0.0, 0.0, 0.0, 3000.0,   0.85, 0.95,   0.00, 0.00, 0.0 },
};

// Tap-Verlauf der Schwapp-Echos (8 Echos): abfallend bzw. anschwellend, Mischung ueber echoSwell
constexpr double kEchoDecay[8] = { 0.60, 0.42, 0.30, 0.21, 0.15, 0.10, 0.07, 0.05 };
constexpr double kEchoSwell[8] = { 0.18, 0.34, 0.52, 0.66, 0.58, 0.40, 0.24, 0.12 };

// Reverse-Hall (Geist): Laenge des Schwells in Sekunden, Abklingrate des Halls (in dB ueber die Laenge)
constexpr double kGhostSeconds = 0.60;
constexpr double kGhostDecayDb = 48.0;
constexpr double kGhostRevEcho = 0.55;     // Geist: Pegel des Reverse-Echos (rueckwaerts gespielte Stuecke, die in die Stimme hineinsaugen)
constexpr double kGhostRevEchoLpHz = 3200.0;

// GlitchShifter (Erinnerung): Tonhoehenabfall und Griffigkeit, feste Werte
constexpr double kGlitchSpeed = -0.035;     // negativ = tiefer (ca. -0,6 Halbtoene)
constexpr double kGlitchTightness = 0.62;   // 0..1; hoeher = haerter geschnitten
constexpr double kGlitchFeedback = 0.10;    // Rueckkopplung 0..0.67
// PitchNasty (Traum): Verstimmung und Koernung, feste Werte
constexpr double kNastyBendSemis = -0.45;   // Verstimmung in Halbtoenen
constexpr double kNastyGrindSemis = -20.0;  // Koernung: Halbtoene unter dem mittleren C (tiefer = groeber)
constexpr double kNastyFeedback = 0.25;     // Rueckkopplung 0..1
// Hall je Modus (Airwindows Galactic): Pegel, Ausklang (Replace: hoch = kurz), Helligkeit, Schwebung, Raumgroesse
struct Room { double level, replace, bright, detune, size; };
static const Room kRoom[kNumModes] = {
    { 0.35, 0.88, 0.55, 0.00, 0.06 },   // Innere Stimme: winzig, eng, nur ein Hauch Raum im Kopf
    { 0.72, 0.62, 0.28, 0.15, 0.30 },   // Erinnerung: kleines, dumpfes, altes Zimmer
    { 1.05, 0.72, 0.32, 0.90, 1.00 },   // Traum: riesig, dunkel, stark schwebend
    { 0.00, 0.65, 0.40, 0.35, 0.55 } }; // Geist: kein Vorwaerts-Hall, nur Reverse (die Stimme bleibt trocken)
constexpr double kFlutterAmount = 0.52;   // Airwindows-Flutter: Tiefe des Bandleierns
constexpr double kVibeDrift = 0.62;       // Airwindows-GalacticVibe: Geschwindigkeit des Waberns
constexpr double kSmoothSeconds = 0.060;    // Zeit, in der Moduswechsel ueberblendet werden

// ======================================================================================================
//  SweetWide (Airwindows, MIT)
// ======================================================================================================
inline void sweetWide (double& l, double& r, double soar, double blend)
{
    double outL = 0.0, outR = 0.0;
    const double inL = std::fabs (l) + (soar * soar);
    const double inR = std::fabs (r) + (soar * soar);
    if (l > 0.0) outL = std::fmax ((std::sqrt (inR / inL) * inL) - soar, 0.0);
    if (l < 0.0) outL = std::fmin ((-std::sqrt (inR / inL) * inL) + soar, 0.0);
    if (r > 0.0) outR = std::fmax ((std::sqrt (inL / inR) * inR) - soar, 0.0);
    if (r < 0.0) outR = std::fmin ((-std::sqrt (inL / inR) * inR) + soar, 0.0);
    l = (outL * blend) + (l * (1.0 - blend));
    r = (outR * blend) + (r * (1.0 - blend));
}

// ======================================================================================================
//  StereoEnsemble (Airwindows, MIT): zwoelf Verzoegerungsleitungen
// ======================================================================================================
class StereoEnsemble
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate / 44100.0;
        for (int i = 0; i < 12; ++i)
        {
            d[i].assign ((size_t) (kBase[i] * rate) + 16, 0.0);
            one[i] = 1;
        }
        setDepth (0.5);
    }
    void reset()
    {
        for (int i = 0; i < 12; ++i) { std::fill (d[i].begin(), d[i].end(), 0.0); one[i] = 1; }
    }
    void setDepth (double depth01)
    {
        const double delayfactor = 0.66 + (depth01 / 3.0);
        for (int i = 0; i < 12; ++i)
            maxDelay[i] = (int) (kBase[i] * delayfactor * rate);
    }
    void process (double& l, double& r, double level)
    {
        const double dryL = l, dryR = r;
        double outL = 0.0, outR = 0.0, temp = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            auto& line = d[i];
            line[(size_t) one[i]] = (i % 2 == 0) ? dryL : dryR;
            --one[i];
            if (one[i] < 0 || one[i] > maxDelay[i]) one[i] = maxDelay[i];
            if (i % 2 == 0) { temp = line[(size_t) one[i]]; outL += temp * kLevel[i]; }
            else            { temp += line[(size_t) one[i]]; outR += temp * kLevel[i]; }
        }
        const double dry = 1.0 - (level * 0.65);
        l = (outL * level) + (dryL * dry);
        r = (outR * level) + (dryR * dry);
    }
private:
    static constexpr double kBase[12] = { 7481.0, 7523.0, 5779.0, 5737.0, 4831.0, 4861.0,
                                          3109.0, 3079.0, 2203.0, 2213.0, 1327.0, 1321.0 };
    static constexpr double kLevel[12] = { 0.75, 0.15, 0.15, 0.75, 0.63, 0.27, 0.27, 0.63, 0.48, 0.32, 0.32, 0.48 };
    std::vector<double> d[12];
    int one[12] {}, maxDelay[12] {};
    double rate = 1.0;
};

// ======================================================================================================
//  GlitchShifter (Airwindows, MIT): schneidet Stuecke an Nulldurchgaengen, ohne zu ueberblenden
// ======================================================================================================
class GlitchShifter
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate / 44100.0;
        for (auto& c : ch) { c = Chan(); c.p.assign (131076, 0); }
        gcount = 0; lastwidth = 0; flip = false;
    }
    void reset() { prepare (rate * 44100.0); }

    // l, r: Eingang, Rueckgabe: Ergebnis (voll "nass")
    void process (double& l, double& r, double speed, double tightness, double feedback)
    {
        int width = (int) (65536 - ((1 - std::pow (1 - tightness, 2)) * 65530.0));
        width = std::max (6, std::min (65530, (int) (width * rate)));
        const double bias = std::pow (tightness, 3);
        if (lastwidth != width)
        {
            for (auto& c : ch) { c.crosses = 0; c.realzeroes = 0; }
            lastwidth = width;
        }
        ++gcount;
        if (gcount < 0 || gcount > width) gcount = 0;
        const double in[2] = { l, r };
        double out[2];
        for (int i = 0; i < 2; ++i)
            out[i] = tick (ch[i], in[i], width, speed, bias, feedback);
        flip = ! flip;
        l = std::max (-4.0, std::min (4.0, out[0]));
        r = std::max (-4.0, std::min (4.0, out[1]));
    }

private:
    struct Chan
    {
        std::vector<int32_t> p;
        int32_t offset[258] {}, pastzero[258] {}, previous[258] {}, third[258] {}, fourth[258] {};
        int32_t temp = 0, lasttemp = 0, thirdtemp = 0, fourthtemp = 0, sincezerocross = 0;
        int crosses = 0, realzeroes = 0;
        double position = 0.0;
        bool splicing = false;
        double airPrev = 0.0, airEven = 0.0, airOdd = 0.0, airFactor = 0.0;
    };

    double tick (Chan& c, double input, int width, double speed, double bias, double feedback)
    {
        // "Air": gleicht den Hoehenverlust der Interpolation aus
        c.airFactor = c.airPrev - input;
        if (flip) { c.airEven += c.airFactor; c.airOdd -= c.airFactor; c.airFactor = c.airEven; }
        else      { c.airOdd += c.airFactor; c.airEven -= c.airFactor; c.airFactor = c.airOdd; }
        c.airOdd = (c.airOdd - ((c.airOdd - c.airEven) / 256.0)) / 1.0001;
        c.airEven = (c.airEven - ((c.airEven - c.airOdd) / 256.0)) / 1.0001;
        c.airPrev = input;
        input += c.airFactor;

        auto wrap = [width] (int v) { while (v < 0) v += width; while (v > width) v -= width; return v; };

        int count = wrap (gcount), countone = wrap (gcount - 1), counttwo = wrap (gcount - 2);
        c.p[(size_t) (count + width)] = c.p[(size_t) count] = (int32_t) ((input * 8388352.0) + (double) (c.lasttemp * feedback));

        const int32_t cur = c.p[(size_t) count], prev = c.p[(size_t) countone];
        if ((prev > 0 && cur < 0) || (prev < 0 && cur > 0))   // Quelle hat Nulldurchgang
        {
            ++c.crosses; ++c.realzeroes;
            if (c.crosses > 256) c.crosses = 0;
            if (c.realzeroes > 256) c.realzeroes = 256;
            c.offset[c.crosses] = count;
            c.pastzero[c.crosses] = cur;
            c.previous[c.crosses] = prev;
            c.third[c.crosses] = c.p[(size_t) counttwo];
        }

        c.position -= speed;

        auto splice = [&]
        {
            c.position = 0;
            double diff = 99999999.0;
            int best = 0;
            for (int scan = c.realzeroes - 1; scan >= 0; --scan)
            {
                int scanone = scan + c.crosses;
                if (scanone > 256) scanone -= 256;
                double howdiff = (double) ((c.temp - c.pastzero[scanone]) + (c.lasttemp - c.previous[scanone])
                                           + (c.thirdtemp - c.third[scanone]) + (c.fourthtemp - c.fourth[scanone]));
                howdiff -= (double) (scan * bias);
                if (howdiff < diff) { diff = howdiff; best = scanone; }
            }
            c.position = c.offset[best] - c.sincezerocross;
            c.crosses = 0; c.realzeroes = 0; c.splicing = true;
        };
        if (c.position > width)
        {
            if (c.realzeroes > 0) splice();
            else { c.position -= width; c.crosses = 0; c.realzeroes = 0; c.splicing = true; }
        }
        if (c.position < 0)
        {
            if (c.realzeroes > 0) splice();
            else { c.position += width; c.crosses = 0; c.realzeroes = 0; c.splicing = true; }
        }
        // Sicherheitsnetz gegen unsinnige Positionen
        if (! (std::fabs (c.position) < 2.0 * (double) width)) c.position = 0.0;

        count = gcount - (int) std::floor (c.position);
        count = wrap (count);
        const double frac = c.position - std::floor (c.position);
        int32_t t = 0;
        t += (int32_t) (c.p[(size_t) count] * (1 - frac));
        t += c.p[(size_t) count + 1];
        t += (int32_t) (c.p[(size_t) count + 2] * frac);
        t -= (int32_t) (((c.p[(size_t) count] - c.p[(size_t) count + 1]) - (c.p[(size_t) count + 1] - c.p[(size_t) count + 2])) / 50);
        t /= 2;
        if (std::abs ((double) t) > 8388352.0) t = c.lasttemp + (c.lasttemp - c.thirdtemp);

        ++c.sincezerocross;
        if (c.sincezerocross < 0 || c.sincezerocross > width) c.sincezerocross = 0;
        if (c.splicing) { t = (t + (c.lasttemp + (c.lasttemp - c.thirdtemp))) / 2; c.splicing = false; }
        if ((c.lasttemp > 0 && t < 0) || (c.lasttemp < 0 && t > 0)) c.sincezerocross = 0;

        c.temp = t;
        c.fourthtemp = c.thirdtemp;
        c.thirdtemp = c.lasttemp;
        c.lasttemp = c.temp;
        return (double) t / 8388352.0;
    }

    Chan ch[2];
    int gcount = 0, lastwidth = 0;
    bool flip = false;
    double rate = 1.0;
};

// ======================================================================================================
//  PitchNasty (Airwindows, MIT): primitive Tonhoehenverschiebung mit Koernung
// ======================================================================================================
class PitchNasty
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }
    void reset()
    {
        std::fill (d[0], d[0] + kSize, 0.0);
        std::fill (d[1], d[1] + kSize, 0.0);
        inCount = 1; outCount = 1.0; switchTransition = false;
        switchAmount[0] = switchAmount[1] = 0.0;
        fb[0] = fb[1] = 0.0;
    }
    void process (double& l, double& r, double bendSemis, double grindSemis, double feedback)
    {
        const double speed = std::pow (2.0, bendSemis / 12.0);
        const double grindRef = (1.0 / 261.6) * sr;                        // Samples je Zyklus des mittleren C
        const double grindNote = std::pow (2.0, grindSemis / 12.0);
        double width = grindRef / grindNote;
        if (width > 9990) width = 9990;
        if (width < 8) width = 8;

        double in[2] = { l, r };
        for (int i = 0; i < 2; ++i)
        {
            in[i] += fb[i] * feedback;
            if (std::fabs (fb[i]) > 1.0) in[i] /= std::fabs (fb[i]);
            d[i][inCount] = in[i];
        }
        ++inCount;
        outCount += speed;
        if (outCount > inCount && switchTransition)
        {
            switchAmount[0] = switchAmount[1] = 1.0;
            switchTransition = false;
        }
        if (inCount > width) inCount -= (int) width;
        if (outCount > width) { outCount -= width; switchTransition = true; }

        const int count = (int) std::floor (outCount);
        const int arrayWidth = (int) std::floor (width);
        const double frac = outCount - std::floor (outCount);
        double out[2];
        for (int i = 0; i < 2; ++i)
        {
            double s = d[i][(size_t) (count - ((count > arrayWidth) ? arrayWidth + 1 : 0))] * (1.0 - frac);
            s += d[i][(size_t) (count + 1 - ((count + 1 > arrayWidth) ? arrayWidth + 1 : 0))] * frac;
            if (switchAmount[i] > std::fabs (s - d[i][inCount]) * 2.0)
                switchAmount[i] = (switchAmount[i] * 0.5) + std::fabs (s - d[i][inCount]);
            s = (d[i][inCount] * switchAmount[i]) + (s * (1.0 - switchAmount[i]));
            fb[i] = s;
            out[i] = s;
        }
        l = out[0]; r = out[1];
    }
private:
    static constexpr int kSize = 10020;
    double d[2][kSize] {};
    int inCount = 1;
    double outCount = 1.0;
    bool switchTransition = false;
    double switchAmount[2] {}, fb[2] {};
    double sr = 44100.0;
};

// ======================================================================================================
//  Kleiner Filter (RBJ-Biquad)
// ======================================================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (bool highpass, double hz, double sampleRate)
    {
        hz = std::max (10.0, std::min (hz, sampleRate * 0.45));
        const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
        const double cs = std::cos (w), alpha = std::sin (w) / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        if (highpass) { b0 = ((1 + cs) / 2) / a0; b1 = -(1 + cs) / a0; b2 = b0; }
        else          { b0 = ((1 - cs) / 2) / a0; b1 = (1 - cs) / a0;  b2 = b0; }
        a1 = (-2 * cs) / a0;
        a2 = (1 - alpha) / a0;
    }
    double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0; }
};

// ======================================================================================================
//  Galactic (Airwindows, Chris Johnson, MIT): Hall aus drei 4x4-Rueckkopplungsnetzen mit schwebendem Vorlauf
//  Portiert ohne Dither. Die Raumgroesse wird beim prepare() fest eingestellt (jeder Modus hat sein eigenes Exemplar).
// ======================================================================================================
class Galactic
{
public:
    void prepare (double sampleRate, const Room& room)
    {
        const double overall = sampleRate / 44100.0;
        cycleEnd = std::max (1, std::min (4, (int) std::floor (overall)));
        regen = 0.0625 + (1.0 - room.replace) * 0.0625;
        attenuate = (1.0 - regen / 0.125) * 1.333;
        lowpass = std::pow (1.00001 - (1.0 - room.bright), 2.0) / std::sqrt (overall);
        drift = std::pow (room.detune, 3.0) * 0.001;
        const double size = room.size * 1.77 + 0.1;
        static const double base[12] = { 3407, 1823, 859, 331, 4801, 2909, 1153, 461, 7607, 4217, 2269, 1597 };
        for (int k = 0; k < 12; ++k)
        {
            d[k] = std::max (1, (int) (base[k] * size));
            for (int c = 0; c < 2; ++c) buf[c][k].assign ((size_t) d[k] + 2, 0.0);
        }
        for (int c = 0; c < 2; ++c) bufM[c].assign ((size_t) kDelayM + 2, 0.0);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& b : buf[c]) std::fill (b.begin(), b.end(), 0.0);
            std::fill (bufM[c].begin(), bufM[c].end(), 0.0);
            for (auto& f : fb[c]) f = 0.0;
            for (auto& r : lastRef[c]) r = 0.0;
            iirA[c] = iirB[c] = 0.0;
        }
        for (auto& n : count) n = 1;
        countM = 1; cycle = 0; vibM = 3.0; oldfpd = 0.5;
    }
    // Eingang l/r, Rueckgabe: nur der Hall (wet)
    void process (double inL, double inR, double& outL, double& outR)
    {
        vibM += oldfpd * drift;
        if (vibM > 6.283185307179586) { vibM = 0.0; rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; oldfpd = 0.4294967295 + rng * 0.0000000000618; }

        bufM[0][(size_t) countM] = inL * attenuate;
        bufM[1][(size_t) countM] = inR * attenuate;
        if (++countM > kDelayM) countM = 0;
        double x[2];
        for (int c = 0; c < 2; ++c)
        {
            const double off = (std::sin (vibM + (c ? 1.5707963267948966 : 0.0)) + 1.0) * 127.0;
            const int w = countM + (int) off;
            const double frac = off - std::floor (off);
            const int i0 = w > kDelayM ? w - (kDelayM + 1) : w;
            const int i1 = (w + 1) > kDelayM ? w + 1 - (kDelayM + 1) : w + 1;
            x[c] = bufM[c][(size_t) i0] * (1.0 - frac) + bufM[c][(size_t) i1] * frac;
            iirA[c] = iirA[c] * (1.0 - lowpass) + x[c] * lowpass;
            x[c] = iirA[c];
        }

        if (++cycle == cycleEnd)
        {
            double o[2][4];
            // erster Block: Eingang + ueberkreuzte Rueckkopplung
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k)
                    buf[c][k][(size_t) count[k]] = x[c] + fb[1 - c][k] * regen;
            for (int k = 0; k < 4; ++k) advance (k);
            for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) o[c][k] = tap (c, k);
            // zweiter und dritter Block
            for (int stage = 1; stage <= 2; ++stage)
            {
                const int b0 = stage * 4;
                for (int c = 0; c < 2; ++c)
                    for (int k = 0; k < 4; ++k)
                        buf[c][b0 + k][(size_t) count[b0 + k]] = householder (o[c], k);
                for (int k = 0; k < 4; ++k) advance (b0 + k);
                for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) o[c][k] = tap (c, b0 + k);
            }
            for (int c = 0; c < 2; ++c)
            {
                for (int k = 0; k < 4; ++k) fb[c][k] = householder (o[c], k);
                const double y = (o[c][0] + o[c][1] + o[c][2] + o[c][3]) / 8.0;
                double* r = lastRef[c];
                if (cycleEnd == 4) { r[0] = r[4]; r[2] = (r[0] + y) / 2; r[1] = (r[0] + r[2]) / 2; r[3] = (r[2] + y) / 2; r[4] = y; }
                else if (cycleEnd == 3) { r[0] = r[3]; r[2] = (r[0] + r[0] + y) / 3; r[1] = (r[0] + y + y) / 3; r[3] = y; }
                else if (cycleEnd == 2) { r[0] = r[2]; r[1] = (r[0] + y) / 2; r[2] = y; }
                else r[0] = y;
            }
            cycle = 0;
        }
        for (int c = 0; c < 2; ++c)
        {
            iirB[c] = iirB[c] * (1.0 - lowpass) + lastRef[c][cycle] * lowpass;
            x[c] = iirB[c];
        }
        outL = x[0]; outR = x[1];
    }

private:
    static constexpr int kDelayM = 256;
    static double householder (const double* o, int k) { return o[k] - (o[0] + o[1] + o[2] + o[3] - o[k]); }
    void advance (int k) { if (++count[k] > d[k]) count[k] = 0; }
    double tap (int c, int k) const
    {
        const int n = count[k];
        return buf[c][k][(size_t) (n > d[k] ? n - d[k] - 1 : n)];
    }

    std::vector<double> buf[2][12], bufM[2];
    int d[12] {}, count[12] {}, countM = 1, cycle = 0, cycleEnd = 1;
    double fb[2][4] {}, lastRef[2][7] {}, iirA[2] {}, iirB[2] {};
    double regen = 0.09, attenuate = 0.5, lowpass = 0.5, drift = 0.0, vibM = 3.0, oldfpd = 0.5;
    uint32_t rng = 0x2545F491u;
};

// ======================================================================================================
//  Flutter (Airwindows, MIT): unregelmaessiges Bandleiern. Portiert ohne Dither, Tiefe/Geschwindigkeit skaliert mit der Abtastrate.
// ======================================================================================================
class Flutter
{
public:
    void prepare (double sampleRate, double amount)
    {
        const double scale = sampleRate / 44100.0;
        depth = amount * amount * scale * 70.0;
        trim = (0.0024 * amount * amount) / scale;
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) std::fill (d[c], d[c] + 1000, 0.0);
        gcount = 999; sweep = 3.141592653589793 * 2.0; rateof = 0.5; nextmax = 0.5;
    }
    void process (double& l, double& r)
    {
        if (gcount < 0 || gcount > 999) gcount = 999;
        d[0][gcount] = l; d[1][gcount] = r;
        const double offset = depth + depth * rateof * rateof * std::sin (sweep);
        const int whole = (int) std::floor (offset);
        const double frac = offset - whole;
        int c0 = gcount + whole; if (c0 > 999) c0 -= 1000;
        int c1 = gcount + whole + 1; if (c1 > 999) c1 -= 1000;
        l = d[0][c0] * (1.0 - frac) + d[0][c1] * frac;
        r = d[1][c0] * (1.0 - frac) + d[1][c1] * frac;
        rateof = rateof * (1.0 - trim) + nextmax * trim;
        sweep += rateof * trim;
        if (sweep >= 6.283185307179586)
        {
            sweep -= 3.141592653589793;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            nextmax = 0.24 + ((double) rng / 4294967295.0) * 0.74;
        }
        --gcount;
    }
private:
    double d[2][1000] {};
    int gcount = 999;
    double depth = 0.0, trim = 0.0, sweep = 6.283185307179586, rateof = 0.5, nextmax = 0.5;
    uint32_t rng = 0x1234567u;
};

// ======================================================================================================
//  GalacticVibe (Airwindows, MIT): langsames, zufaelliges Wabern der Tonhoehe ueber einen kurzen schwebenden Vorlauf
// ======================================================================================================
class GalacticVibe
{
public:
    void prepare (double sampleRate, double drift)
    {
        scale = sampleRate / 44100.0;
        delayM = std::max (8, (int) std::lround (256.0 * scale));
        driftK = std::pow (drift, 3.0) * 0.001 / scale;
        for (int c = 0; c < 2; ++c) buf[c].assign ((size_t) delayM + 2, 0.0);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) std::fill (buf[c].begin(), buf[c].end(), 0.0);
        countM = 1; vibM = 3.0; oldfpd = 0.5;
    }
    void process (double& l, double& r)
    {
        vibM += oldfpd * driftK;
        if (vibM > 6.283185307179586)
        {
            vibM = 0.0;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            oldfpd = 0.4294967295 + (double) rng * 0.0000000000618;
        }
        buf[0][(size_t) countM] = l; buf[1][(size_t) countM] = r;
        if (++countM > delayM) countM = 0;
        const double half = 0.5 * delayM;
        double out[2];
        for (int c = 0; c < 2; ++c)
        {
            const double off = (std::sin (vibM + (c ? 1.5707963267948966 : 0.0)) + 1.0) * half;
            const int w = countM + (int) off;
            const double frac = off - std::floor (off);
            const int i0 = w > delayM ? w - (delayM + 1) : w;
            const int i1 = (w + 1) > delayM ? w + 1 - (delayM + 1) : w + 1;
            out[c] = buf[c][(size_t) i0] * (1.0 - frac) + buf[c][(size_t) i1] * frac;
        }
        l = out[0]; r = out[1];
    }
private:
    std::vector<double> buf[2];
    double scale = 1.0, driftK = 0.0, vibM = 3.0, oldfpd = 0.5;
    int delayM = 256, countM = 1;
    uint32_t rng = 0x7654321u;
};

// ======================================================================================================
//  Verzoegerungsring (feste Laenge, Rueckgabe = Wert von vor genau n Samples)
// ======================================================================================================
struct Ring
{
    std::vector<double> b;
    int pos = 0;
    void init (int n) { b.assign ((size_t) std::max (1, n), 0.0); pos = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0); pos = 0; }
    double tick (double x)
    {
        const double y = b[(size_t) pos];
        b[(size_t) pos] = x;
        if (++pos == (int) b.size()) pos = 0;
        return y;
    }
};

// ======================================================================================================
//  Reverse-Hall (Geist): Faltung mit einem rueckwaerts laufenden Hall-Ausklang
//
//  Ein normaler Hall klingt nach dem Ton aus. Dreht man den Ausklang um, schwillt der Hall VOR dem Ton an.
//  Das Plugin macht genau das in Echtzeit: Es faltet die Stimme mit einem umgedrehten, nach oben gerichteten
//  Hall-Ausklang (Rauschen mit abnehmender Daempfung der Hoehen). Der Schwell erreicht seinen Hoehepunkt erst
//  kGhostSeconds nach dem Ton, darum wird die Stimme um genau diese Zeit verzoegert; die DAW gleicht diese Latenz aus,
//  dann liegt der Schwell hoerbar vor der Stimme.
// ======================================================================================================
class SwellConvolver
{
public:
    static constexpr int B = 512;          // Blockgroesse (Teil der Latenz)
    static constexpr int F = 1024;         // FFT-Groesse = 2 * B

    void prepare (double sampleRate)
    {
        irLen = std::max (B, (int) std::lround (kGhostSeconds * sampleRate));
        parts = (irLen + B - 1) / B;
        initFft();
        for (int c = 0; c < 2; ++c)
        {
            std::vector<double> h = makeImpulseResponse (sampleRate, c == 0 ? 0x9E3779B9u : 0x85EBCA6Bu);
            H[c].assign ((size_t) parts * F, Cx {});
            for (int p = 0; p < parts; ++p)
            {
                Cx* dst = &H[c][(size_t) p * F];
                for (int i = 0; i < B; ++i)
                {
                    const int k = p * B + i;
                    dst[i].r = k < irLen ? h[(size_t) k] : 0.0;
                }
                fft (dst, false);
            }
        }
        X.assign ((size_t) parts * F, Cx {});
        reset();
    }
    void reset()
    {
        std::fill (X.begin(), X.end(), Cx {});
        std::fill (prev, prev + B, 0.0);
        std::fill (inBlk, inBlk + B, 0.0);
        std::fill (out[0], out[0] + B, 0.0);
        std::fill (out[1], out[1] + B, 0.0);
        head = 0; fill = 0;
    }
    // Zeit vom Eingang bis zum Hoehepunkt des Schwells: so lange muss die Stimme verzoegert werden
    int latency() const { return irLen + B; }

    // m: Eingangswert (mono). Rueckgabe: Schwell links/rechts (um B Samples verzoegert, in latency() mit eingerechnet)
    void tick (double m, double& wl, double& wr)
    {
        wl = out[0][fill];
        wr = out[1][fill];
        inBlk[fill] = m;
        if (++fill == B) { runBlock(); fill = 0; }
    }

private:
    struct Cx { double r = 0.0, i = 0.0; };

    static uint32_t xorshift (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

    std::vector<double> makeImpulseResponse (double sr, uint32_t seed) const
    {
        // natuerliche Richtung (s = Zeit seit dem Anfang des Halls): Rauschen, Hoehen sterben schneller, Pegel faellt ab
        std::vector<double> nat ((size_t) irLen);
        const double tau = (double) irLen / (kGhostDecayDb / 8.685889638);   // exp(-T/tau) = -kGhostDecayDb dB
        double lp = 0.0;
        const int fadeIn = std::max (1, (int) (0.003 * sr));
        for (int s = 0; s < irLen; ++s)
        {
            const double noise = ((double) xorshift (seed) / 4294967295.0) * 2.0 - 1.0;
            const double a = 0.10 + 0.55 * (1.0 - (double) s / (double) irLen);
            lp += a * (noise - lp);
            double v = lp * std::exp (-(double) s / tau);
            if (s < fadeIn) v *= (double) s / (double) fadeIn;
            nat[(size_t) s] = v;
        }
        std::vector<double> h ((size_t) irLen);
        double energy = 0.0;
        for (int k = 0; k < irLen; ++k) { h[(size_t) k] = nat[(size_t) (irLen - 1 - k)]; energy += h[(size_t) k] * h[(size_t) k]; }
        const double norm = energy > 0.0 ? 1.0 / std::sqrt (energy) : 1.0;
        for (auto& v : h) v *= norm;
        return h;
    }

    void initFft()
    {
        rev.assign (F, 0);
        int bits = 0; while ((1 << bits) < F) ++bits;
        for (int i = 0; i < F; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        tw.assign (F / 2, Cx {});
        for (int k = 0; k < F / 2; ++k) { const double a = -2.0 * 3.14159265358979323846 * k / F; tw[(size_t) k].r = std::cos (a); tw[(size_t) k].i = std::sin (a); }
    }
    void fft (Cx* a, bool inverse) const
    {
        for (int i = 0; i < F; ++i) { const int j = rev[(size_t) i]; if (i < j) std::swap (a[i], a[j]); }
        for (int len = 2; len <= F; len <<= 1)
        {
            const int half = len / 2, step = F / len;
            for (int i = 0; i < F; i += len)
                for (int k = 0; k < half; ++k)
                {
                    Cx w = tw[(size_t) (k * step)];
                    if (inverse) w.i = -w.i;
                    const Cx u = a[i + k], t = a[i + k + half];
                    const Cx v { t.r * w.r - t.i * w.i, t.r * w.i + t.i * w.r };
                    a[i + k] = { u.r + v.r, u.i + v.i };
                    a[i + k + half] = { u.r - v.r, u.i - v.i };
                }
        }
        if (inverse) { const double g = 1.0 / F; for (int i = 0; i < F; ++i) { a[i].r *= g; a[i].i *= g; } }
    }

    void runBlock()
    {
        // Overlap-Save: [vorheriger Block | neuer Block]
        Cx* slot = &X[(size_t) head * F];
        for (int i = 0; i < B; ++i) { slot[i] = { prev[i], 0.0 }; slot[B + i] = { inBlk[i], 0.0 }; prev[i] = inBlk[i]; }
        fft (slot, false);
        for (int c = 0; c < 2; ++c)
        {
            std::fill (acc, acc + F, Cx {});
            for (int p = 0; p < parts; ++p)
            {
                const Cx* x = &X[(size_t) ((head - p + parts) % parts) * F];
                const Cx* h = &H[c][(size_t) p * F];
                for (int k = 0; k <= F / 2; ++k)   // reelle Signale: obere Haelfte ist gespiegelt
                {
                    acc[k].r += x[k].r * h[k].r - x[k].i * h[k].i;
                    acc[k].i += x[k].r * h[k].i + x[k].i * h[k].r;
                }
            }
            for (int k = 1; k < F / 2; ++k) acc[F - k] = { acc[k].r, -acc[k].i };
            fft (acc, true);
            for (int i = 0; i < B; ++i) out[c][i] = acc[B + i].r;
        }
        head = (head + 1) % parts;
    }

    int irLen = B, parts = 1, head = 0, fill = 0;
    std::vector<int> rev;
    std::vector<Cx> tw, H[2], X;
    double prev[B] {}, inBlk[B] {}, out[2][B] {};
    Cx acc[F];
};

// ======================================================================================================
//  Die ganze Kette
// ======================================================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smooth = 1.0 - std::exp (-1.0 / (kSmoothSeconds * sr));
        fader = 1.0 - std::exp (-1.0 / (0.020 * sr));
        ens.prepare (sr); glitch.prepare (sr); nasty.prepare (sr);
        flutter.prepare (sr, kFlutterAmount); vibe.prepare (sr, kVibeDrift);
        int n = 1; while ((double) n < 3.4 * sr) n <<= 1;
        echoBuf.assign ((size_t) n, 0.0); echoMask = n - 1; echoPos = 0;
        swell.prepare (sr);
        sideDelay = std::min (60.0, 0.00045 * sr);   // ca. 0,45 ms wie bei Wider (20 Samples bei 44,1 kHz)
        for (int m = 0; m < kNumModes; ++m) room[m].prepare (sr, kRoom[m]);
        ghostDelay = swell.latency();
        for (auto* r : { &dlL, &dlR, &dryL, &dryR }) r->init (ghostDelay);
        {
            int m = 1; while (m < ghostDelay + 4) m <<= 1;
            revBuf.assign ((size_t) m, 0.0); revMask = m - 1; revPos = 0;
            revLen = std::max (64, ghostDelay / 2);   // Stuecklaenge: endet genau dort, wo die (verzoegerte) Stimme ist
            revCoef = 1.0 - std::exp (-2.0 * 3.14159265358979323846 * kGhostRevEchoLpHz / sr);
        }
        reset();
        snapToMode (mode);
    }
    void reset()
    {
        ens.reset(); glitch.reset(); nasty.reset(); swell.reset(); flutter.reset(); vibe.reset();
        for (auto& f : hp) f.clear();
        for (auto& f : lp) f.clear();
        std::fill (echoBuf.begin(), echoBuf.end(), 0.0);
        echoLpL = echoLpR = 0.0;
        for (auto* r : { &dlL, &dlR, &dryL, &dryR }) r->clear();
        std::fill (revBuf.begin(), revBuf.end(), 0.0); revPos = 0; revPhase = 0; revLpL = revLpR = 0.0;
        ghostRunning = false;
        std::fill (sideBuf, sideBuf + 64, 0.0); sidePos = 0;
        for (int m = 0; m < kNumModes; ++m) { room[m].reset(); roomRunning[m] = false; }
    }
    void setMode (int m) { mode = std::max (0, std::min (kNumModes - 1, m)); }
    // Latenz, die dem Host gemeldet werden muss: nur im Modus Geist (Schwell liegt VOR der Stimme)
    int latencySamples() const { return mode == kGhost ? ghostDelay : 0; }

    // dream: 0..1 (0 = unbearbeitete Stimme, 1 = voller Klang)
    // hall, echo: 0..2 (1 = so, wie der Modus abgestimmt ist)
    void process (float* left, float* right, int n, double dream, double hall = 1.0, double echo = 1.0, double width = 1.0)
    {
        const Voicing& v = kVoicing[mode];
        if (! started) { amount = dream; hallK = hall; echoK = echo; widthK = width; started = true; }
        const double ghostTarget = mode == kGhost ? 1.0 : 0.0;
        for (int i = 0; i < n; ++i)
        {
            cur.glitchWet += (v.glitchWet - cur.glitchWet) * smooth;
            cur.nastyWet += (v.nastyWet - cur.nastyWet) * smooth;
            cur.wideSoar += (v.wideSoar - cur.wideSoar) * smooth;
            cur.wideBlend += (v.wideBlend - cur.wideBlend) * smooth;
            cur.ensDepth += (v.ensDepth - cur.ensDepth) * smooth;
            cur.ensLevel += (v.ensLevel - cur.ensLevel) * smooth;
            cur.highpassHz += (v.highpassHz - cur.highpassHz) * smooth;
            cur.lowpassHz += (v.lowpassHz - cur.lowpassHz) * smooth;
            cur.trim += (v.trim - cur.trim) * smooth;
            cur.flutterWet += (v.flutterWet - cur.flutterWet) * smooth;
            cur.vibeWet += (v.vibeWet - cur.vibeWet) * smooth;
            amount += (dream - amount) * fader;
            hallK += (hall - hallK) * fader;
            echoK += (echo - echoK) * fader;
            widthK += (width - widthK) * fader;
            ghostAmt += (ghostTarget - ghostAmt) * smooth;

            // Echo-Zeiten gehoeren zum Modus: Echos erst ausblenden, dann Zeiten wechseln (kein Pitch-Rutschen)
            const double echoTarget = (echoMode == mode) ? kVoicing[echoMode].echoLevel * echoK : 0.0;
            echoLevel += (echoTarget - echoLevel) * smooth;
            if (echoMode != mode && echoLevel < 0.002) setEchoMode (mode);

            if ((counter++ & 15) == 0)
            {
                ens.setDepth (cur.ensDepth);
                for (auto& f : hp) f.set (true, cur.highpassHz, sr);
                for (auto& f : lp) f.set (false, cur.lowpassHz, sr);
            }

            const double rawL = left[i], rawR = right != nullptr ? right[i] : left[i];
            double l = rawL, r = rawR;

            // 1. Tonhoehen- und Stotterbausteine laufen immer mit (frische Puffer), der Anteil wird ueberblendet
            double gl = l, gr = r;
            glitch.process (gl, gr, kGlitchSpeed, kGlitchTightness, kGlitchFeedback);
            l = l * (1.0 - cur.glitchWet) + gl * cur.glitchWet;
            r = r * (1.0 - cur.glitchWet) + gr * cur.glitchWet;

            double nl = l, nr = r;
            nasty.process (nl, nr, kNastyBendSemis, kNastyGrindSemis, kNastyFeedback);
            l = l * (1.0 - cur.nastyWet) + nl * cur.nastyWet;
            r = r * (1.0 - cur.nastyWet) + nr * cur.nastyWet;

            // 1b. Schwimmen: Bandleiern (Erinnerung) und langsames Wabern der Tonhoehe (Traum)
            {
                double fl = l, fr = r;
                flutter.process (fl, fr);
                l = l * (1.0 - cur.flutterWet) + fl * cur.flutterWet;
                r = r * (1.0 - cur.flutterWet) + fr * cur.flutterWet;
                double vl = l, vr = r;
                vibe.process (vl, vr);
                l = l * (1.0 - cur.vibeWet) + vl * cur.vibeWet;
                r = r * (1.0 - cur.vibeWet) + vr * cur.vibeWet;
            }

            // 2. Schwapp-Echo: Pre-Delay, dann Ping-Pong-Echos (abfallend oder anschwellend)
            const double mono = 0.5 * (l + r);
            echoBuf[(size_t) (echoPos & echoMask)] = mono;
            double eL = 0.0, eR = 0.0;
            if (echoLevel > 0.0005)
            {
                const Voicing& ev = kVoicing[echoMode];
                for (int k = 1; k <= 8; ++k)
                {
                    const double g = kEchoDecay[k - 1] * (1.0 - ev.echoSwell) + kEchoSwell[k - 1] * ev.echoSwell;
                    const double other = g * (1.0 - ev.echoWidth);
                    double t;
                    if (echoDrift > 0.0)
                    {
                        // jede Wiederholung wandert langsam und eigenstaendig in der Tonhoehe
                        const double dly = (double) (echoPre + k * echoSpace) + echoDrift * std::sin (echoPh[k]);
                        echoPh[k] += echoRate[k];
                        const double fd = std::floor (dly);
                        const double fr = dly - fd;
                        const int n0 = echoPos - (int) fd;
                        t = echoBuf[(size_t) (n0 & echoMask)] * (1.0 - fr) + echoBuf[(size_t) ((n0 - 1) & echoMask)] * fr;
                    }
                    else
                        t = echoBuf[(size_t) ((echoPos - echoPre - k * echoSpace) & echoMask)];
                    if (k & 1) { eL += g * t; eR += other * t; }
                    else       { eR += g * t; eL += other * t; }
                }
                echoLpL += echoCoef * (eL - echoLpL);
                echoLpR += echoCoef * (eR - echoLpR);
            }
            else { echoLpL += echoCoef * (0.0 - echoLpL); echoLpR += echoCoef * (0.0 - echoLpR); }
            ++echoPos;

            // 3. Breite und Echo-Wolke
            if (cur.wideSoar > 0.0005) sweetWide (l, r, cur.wideSoar, cur.wideBlend);
            ens.process (l, r, std::min (1.0, cur.ensLevel * echoK));
            l += echoLpL * echoLevel;
            r += echoLpR * echoLevel;

            // 4. Klangfarbe
            l = lp[0].tick (hp[0].tick (l)) * cur.trim;
            r = lp[1].tick (hp[1].tick (r)) * cur.trim;

            // 5. Geist: Reverse-Hall. Die Stimme wird um die Schwell-Laenge verzoegert, der Schwell liegt davor.
            double outDryL = rawL, outDryR = rawR;
            const double dgl = dlL.tick (l), dgr = dlR.tick (r);        // verzoegerte Stimme (laeuft immer mit)
            const double drl = dryL.tick (rawL), drr = dryR.tick (rawR);
            if (ghostTarget > 0.0 && ! ghostRunning) { swell.reset(); std::fill (revBuf.begin(), revBuf.end(), 0.0); revLpL = revLpR = 0.0; ghostRunning = true; }
            if (ghostRunning)
            {
                double wl = 0.0, wr = 0.0;
                const double gm = 0.5 * (l + r);
                swell.tick (gm, wl, wr);
                // Reverse-Echo: Stuecke der kommenden Stimme werden rueckwaerts gespielt und enden genau am Wort
                revBuf[(size_t) (revPos & revMask)] = gm;
                auto grain = [this] (int k)
                {
                    const double w = std::sin (3.14159265358979323846 * (double) k / (double) revLen);
                    return w * w * revBuf[(size_t) ((revPos - 2 * k) & revMask)];
                };
                const int h = revLen / 2, q = revLen / 4;
                const int k1 = revPhase, k2 = (revPhase + h) % revLen, k3 = (revPhase + q) % revLen, k4 = (k3 + h) % revLen;
                revLpL += revCoef * (grain (k1) + grain (k2) - revLpL);
                revLpR += revCoef * (grain (k3) + grain (k4) - revLpR);
                ++revPos; if (++revPhase >= revLen) revPhase = 0;
                const double re = kGhostRevEcho * echoK;
                const Voicing& gv = kVoicing[kGhost];
                const double gL = dgl * gv.ghostDry + wl * gv.ghostWet * hallK + revLpL * re;
                const double gR = dgr * gv.ghostDry + wr * gv.ghostWet * hallK + revLpR * re;
                l = l * (1.0 - ghostAmt) + gL * ghostAmt;
                r = r * (1.0 - ghostAmt) + gR * ghostAmt;
                outDryL = rawL * (1.0 - ghostAmt) + drl * ghostAmt;
                outDryR = rawR * (1.0 - ghostAmt) + drr * ghostAmt;
                if (ghostTarget == 0.0 && ghostAmt < 0.0005) ghostRunning = false;
            }

            // 6. Hall: jeder Modus hat seinen eigenen Raum; beim Wechsel klingt der alte aus, der neue blendet ein
            {
                double hl = 0.0, hr = 0.0;
                for (int m = 0; m < kNumModes; ++m)
                {
                    const double target = m == mode ? kRoom[m].level * hallK : 0.0;
                    roomLevel[m] += (target - roomLevel[m]) * smooth;
                    if (! roomRunning[m])
                    {
                        if (target == 0.0) continue;
                        room[m].reset(); roomRunning[m] = true;
                    }
                    double wl, wr;
                    room[m].process (l, r, wl, wr);
                    hl += wl * roomLevel[m]; hr += wr * roomLevel[m];
                    if (target == 0.0 && roomLevel[m] < 0.0005) roomRunning[m] = false;
                }
                l += hl; r += hr;
            }

            // 7. Breite (Mitte/Seite): 0 = mono, 1 = wie abgestimmt, 2 = doppelte Seite plus kurz verzoegerte Seite
            //    (Prinzip aus Airwindows Wider): wirkt breiter als die Lautsprecher
            {
                const double mid = 0.5 * (l + r), side = 0.5 * (l - r);
                sideBuf[(size_t) (sidePos & 63)] = side;
                double sd = 0.0;
                if (widthK > 1.0)
                {
                    const int i0 = (int) sideDelay; const double fr = sideDelay - i0;
                    sd = sideBuf[(size_t) ((sidePos - i0) & 63)] * (1.0 - fr) + sideBuf[(size_t) ((sidePos - i0 - 1) & 63)] * fr;
                }
                ++sidePos;
                const double s2 = side * widthK + (widthK > 1.0 ? (widthK - 1.0) * 0.7 * sd : 0.0);
                l = mid + s2; r = mid - s2;
            }

            // 8. Fader: unbearbeitet <-> voller Effekt
            l = safe (outDryL * (1.0 - amount) + l * amount);
            r = safe (outDryR * (1.0 - amount) + r * amount);
            left[i] = (float) l;
            if (right != nullptr) right[i] = (float) r;
        }
    }

private:
    // Schutz vor Ausreissern (NaN/Unendlich/Riesenwerte); normale Pegel bleiben unangetastet (Float-Kette)
    static double safe (double x)
    {
        if (! std::isfinite (x)) return 0.0;
        return std::max (-16.0, std::min (16.0, x));
    }
    void setEchoMode (int m)
    {
        echoMode = m;
        const Voicing& ev = kVoicing[m];
        echoPre = std::max (1, (int) std::lround (ev.echoPreMs * 0.001 * sr));
        echoSpace = std::max (1, (int) std::lround (ev.echoSpaceMs * 0.001 * sr));
        echoCoef = 1.0 - std::exp (-2.0 * 3.14159265358979323846 * ev.echoLpHz / sr);
        echoDrift = ev.echoDriftMs * 0.001 * sr;
        for (int k = 1; k <= 8; ++k) echoRate[k] = 2.0 * 3.14159265358979323846 * (0.11 + 0.043 * k) / sr;
    }
    void snapToMode (int m)
    {
        cur = kVoicing[std::max (0, std::min (kNumModes - 1, m))];
        ens.setDepth (cur.ensDepth);
        for (auto& f : hp) f.set (true, cur.highpassHz, sr);
        for (auto& f : lp) f.set (false, cur.lowpassHz, sr);
        setEchoMode (std::max (0, std::min (kNumModes - 1, m)));
        echoLevel = kVoicing[echoMode].echoLevel;
        ghostAmt = m == kGhost ? 1.0 : 0.0;
        for (int k = 0; k < kNumModes; ++k) roomLevel[k] = k == m ? kRoom[k].level : 0.0;
        amount = 1.0;
        started = false;
    }

    double sr = 44100.0, smooth = 0.001, fader = 0.001, amount = 1.0, ghostAmt = 0.0;
    int mode = kInner, echoMode = kInner;
    bool started = false, ghostRunning = false;
    unsigned counter = 0;
    Voicing cur = kVoicing[kInner];
    StereoEnsemble ens;
    GlitchShifter glitch;
    Flutter flutter;
    GalacticVibe vibe;
    double echoPh[9] {}, echoRate[9] {};
    double echoDrift = 0.0;
    PitchNasty nasty;
    Biquad hp[2], lp[2];
    // Echo
    std::vector<double> echoBuf;
    int echoMask = 0, echoPos = 0, echoPre = 1, echoSpace = 1;
    double echoLevel = 0.0, echoCoef = 0.1, echoLpL = 0.0, echoLpR = 0.0;
    // Geist
    SwellConvolver swell;
    // Hall
    Galactic room[kNumModes];
    double roomLevel[kNumModes] {};
    bool roomRunning[kNumModes] {};
    Ring dlL, dlR, dryL, dryR;
    int ghostDelay = 1;
    std::vector<double> revBuf;
    int revMask = 0, revPos = 0, revPhase = 0, revLen = 64;
    double revCoef = 0.3, revLpL = 0.0, revLpR = 0.0;
    // Regler Hall / Echo (geglaettet)
    double hallK = 1.0, echoK = 1.0, widthK = 1.0;
    double sideBuf[64] {}, sideDelay = 20.0;
    int sidePos = 0;
};
}  // namespace dch
