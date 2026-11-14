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
enum Mode { kInner = 0, kMemory = 1, kDream = 2, kNumModes };

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
};

// Reihenfolge wie enum Mode
static const Voicing kVoicing[kNumModes] = {
    // innere Stimme: nah, im Kopf, leicht verbreitert, ein Hauch Nachhall-Echos, nur wenig dunkler
    { 0.00, 0.00, 0.12, 0.25, 0.35, 0.22, 140.0, 7500.0, 1.25 },
    // Erinnerung: stotternde, leicht abgesunkene Stimme, duenn und dumpf wie aus der Ferne der Zeit
    { 0.75, 0.00, 0.20, 0.20, 0.55, 0.35, 220.0, 4800.0, 2.00 },
    // Traum: koernige, leicht verstimmte Stimme in einer weiten Wolke aus Echos
    { 0.00, 0.60, 0.20, 0.30, 0.85, 0.70, 160.0, 4200.0, 1.80 },
};

// GlitchShifter (Erinnerung): Tonhoehenabfall und Griffigkeit, feste Werte
constexpr double kGlitchSpeed = -0.035;     // negativ = tiefer (ca. -0,6 Halbtoene)
constexpr double kGlitchTightness = 0.62;   // 0..1; hoeher = haerter geschnitten
constexpr double kGlitchFeedback = 0.10;    // Rueckkopplung 0..0.67
// PitchNasty (Traum): Verstimmung und Koernung, feste Werte
constexpr double kNastyBendSemis = -0.45;   // Verstimmung in Halbtoenen
constexpr double kNastyGrindSemis = -20.0;  // Koernung: Halbtoene unter dem mittleren C (tiefer = groeber)
constexpr double kNastyFeedback = 0.25;     // Rueckkopplung 0..1
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
        reset();
        snapToMode (mode);
    }
    void reset()
    {
        ens.reset(); glitch.reset(); nasty.reset();
        for (auto& f : hp) f.clear();
        for (auto& f : lp) f.clear();
    }
    void setMode (int m) { mode = std::max (0, std::min (kNumModes - 1, m)); }

    // dream: 0..1 (0 = unbearbeitete Stimme, 1 = voller Klang)
    void process (float* left, float* right, int n, double dream)
    {
        const Voicing& v = kVoicing[mode];
        if (! started) { amount = dream; started = true; }
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
            amount += (dream - amount) * fader;

            if ((counter++ & 15) == 0)
            {
                ens.setDepth (cur.ensDepth);
                for (auto& f : hp) f.set (true, cur.highpassHz, sr);
                for (auto& f : lp) f.set (false, cur.lowpassHz, sr);
            }

            const double dryL = left[i], dryR = right != nullptr ? right[i] : left[i];
            double l = dryL, r = dryR;

            // 1. Tonhoehen- und Stotterbausteine laufen immer mit (frische Puffer), der Anteil wird ueberblendet
            double gl = l, gr = r;
            glitch.process (gl, gr, kGlitchSpeed, kGlitchTightness, kGlitchFeedback);
            l = l * (1.0 - cur.glitchWet) + gl * cur.glitchWet;
            r = r * (1.0 - cur.glitchWet) + gr * cur.glitchWet;

            double nl = l, nr = r;
            nasty.process (nl, nr, kNastyBendSemis, kNastyGrindSemis, kNastyFeedback);
            l = l * (1.0 - cur.nastyWet) + nl * cur.nastyWet;
            r = r * (1.0 - cur.nastyWet) + nr * cur.nastyWet;

            // 2. Breite und Echo-Wolke
            if (cur.wideSoar > 0.0005) sweetWide (l, r, cur.wideSoar, cur.wideBlend);
            ens.process (l, r, cur.ensLevel);

            // 3. Klangfarbe
            l = lp[0].tick (hp[0].tick (l)) * cur.trim;
            r = lp[1].tick (hp[1].tick (r)) * cur.trim;

            // 4. Fader: unbearbeitet <-> voller Effekt
            l = safe (dryL * (1.0 - amount) + l * amount);
            r = safe (dryR * (1.0 - amount) + r * amount);
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
    void snapToMode (int m)
    {
        cur = kVoicing[std::max (0, std::min (kNumModes - 1, m))];
        ens.setDepth (cur.ensDepth);
        for (auto& f : hp) f.set (true, cur.highpassHz, sr);
        for (auto& f : lp) f.set (false, cur.lowpassHz, sr);
        amount = 1.0;
        started = false;
    }

    double sr = 44100.0, smooth = 0.001, fader = 0.001, amount = 1.0;
    int mode = kInner;
    bool started = false;
    unsigned counter = 0;
    Voicing cur = kVoicing[kInner];
    StereoEnsemble ens;
    GlitchShifter glitch;
    PitchNasty nasty;
    Biquad hp[2], lp[2];
};
}  // namespace dch
