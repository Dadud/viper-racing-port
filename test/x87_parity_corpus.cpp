// Deterministic, asset-free floating-point diagnostic for i386 versus x86-64.
// This is not a physics regression test and QEMU i386 is not a native oracle.
// GCC 15, x86 hosts:
//   g++ -m32 -O2 -std=c++17 -mfpmath=387 -fexcess-precision=standard -ffp-contract=off test/x87_parity_corpus.cpp -o x87-32 -lm
//   g++ -m64 -O2 -std=c++17 -mfpmath=387 -fexcess-precision=standard -ffp-contract=off test/x87_parity_corpus.cpp -o x87-64 -lm
// Run on a native i386-capable kernel and native x86-64; compare `case` lines.
// Input and output values are raw IEEE-754 bit patterns. Exception masks are
// enabled during the corpus so exceptional inputs cannot terminate the process.
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if !(defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
#error This diagnostic requires GCC-compatible inline assembly on x86.
#endif

namespace {
uint16_t x87_control() { uint16_t x; __asm__ volatile("fnstcw %0" : "=m"(x)); return x; }
uint16_t x87_status() { uint16_t x; __asm__ volatile("fnstsw %0" : "=am"(x)); return x; }
uint32_t mxcsr() { uint32_t x; __asm__ volatile("stmxcsr %0" : "=m"(x)); return x; }
void set_x87(uint16_t x) { __asm__ volatile("fldcw %0" : : "m"(x)); }
void set_mxcsr(uint32_t x) { __asm__ volatile("ldmxcsr %0" : : "m"(x)); }
float from_bits(uint32_t raw) { float x; std::memcpy(&x, &raw, sizeof x); return x; }
uint32_t bits(float x) { uint32_t raw; std::memcpy(&raw, &x, sizeof raw); return raw; }
// Integer arguments keep the test input bit patterns independent of floating
// argument registers. Volatile loads/stores keep the arithmetic observable.
__attribute__((noinline)) uint32_t sample(uint32_t a, uint32_t b, uint32_t c) {
    volatile float x=from_bits(a), y=from_bits(b), z=from_bits(c);
    volatile float result = static_cast<float>((static_cast<double>(x) * static_cast<double>(y) + static_cast<double>(z)) /
                                                (static_cast<double>(y) + static_cast<double>(1.000001f)));
    return bits(result);
}
uint32_t random_word(uint32_t& state) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
}
int main() {
    std::fenv_t saved;
    if (std::fegetenv(&saved)) return 2;
    const uint16_t original_cw=x87_control();
    const uint32_t original_mxcsr=mxcsr();
    // Inexact, denormal, zeros, infinities, quiet/signalling NaNs and large values.
    const uint32_t special[]={0u,0x80000000u,0x3f800000u,0xbf800000u,1u,0x80000001u,
        0x007fffffu,0x00800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
        0x7fc12345u,0x7f812345u,0x3f000001u};
    const int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    const uint16_t precisions[]={0x0000,0x0200,0x0300}; // x87: 24, 53, 64 mantissa bits
    int result=0;
    for (unsigned p=0; p<3 && !result; ++p) for (unsigned r=0; r<4 && !result; ++r) {
        if (std::fesetround(rounds[r])) { result=3; break; }
        uint16_t control=x87_control();
        control=static_cast<uint16_t>((control & ~uint16_t(0x0300)) | precisions[p] | 0x003f);
        set_x87(control);
        // Mask SSE exceptions and clear FTZ/DAZ so subnormal inputs are observed.
        uint32_t sse=mxcsr(); sse=(sse | 0x1f80u) & ~uint32_t(0x8040u); set_mxcsr(sse);
        uint32_t rng=0x13579bdfu;
        const unsigned count=sizeof(special)/sizeof(special[0]);
        for (unsigned i=0; i<count*count+512; ++i) {
            uint32_t a,b,c;
            if (i<count*count) { a=special[i/count]; b=special[i%count]; c=special[(i*7+3)%count]; }
            else { a=random_word(rng); b=random_word(rng); c=random_word(rng); }
            std::feclearexcept(FE_ALL_EXCEPT);
            // FE_ALL_EXCEPT need not include x87 denormal-operand status.
            __asm__ volatile("fnclex");
            set_mxcsr(mxcsr() & ~uint32_t(0x3f));
            const uint32_t output=sample(a,b,c);
            const uint16_t status=x87_status(); const uint32_t sse_after=mxcsr();
            std::printf("case pc=%u round=%u i=%u a=%08x b=%08x c=%08x out=%08x x87=%02x mxcsr=%02x\n",
                        p,r,i,a,b,c,output,unsigned(status & 0x3f),unsigned(sse_after & 0x3f));
        }
    }
    if (std::fesetenv(&saved)) result=4;
    // Explicitly restore MXCSR as well: implementations differ in fenv coverage.
    set_mxcsr(original_mxcsr); set_x87(original_cw);
    if (x87_control()!=original_cw || mxcsr()!=original_mxcsr) result=5;
    if (std::ferror(stdout)) result=6;
    return result;
}
