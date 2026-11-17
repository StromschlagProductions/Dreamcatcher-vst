#include "DreamcatcherDSP.h"
#include "TestSupport.h"
#include <memory>
#include <limits>
static std::vector<float> render(double fs,int mode,bool split)
{
 auto e=std::make_unique<dch::Engine>();e->setMode(mode);e->prepare(fs);
 std::vector<float> l(17003),r(l.size());for(size_t i=0;i<l.size();++i)l[i]=r[i]=float(.1*sin(i*.03));
 for(size_t at=0;at<l.size();){int n=(int)std::min(l.size()-at,split?size_t(137):l.size());{NoAllocations guard;e->process(l.data()+at,r.data()+at,n,1,1,1,1,1);}at+=(size_t)n;}
 for(float x:l)CHECK(std::isfinite(x)&&std::abs(x)<16.01f);return l;
}
int main(){NoDenormals denormals;
 for(double fs:{22050.,44100.,48000.,96000.,192000.})for(int mode=0;mode<dch::kNumModes;++mode)CHECK(render(fs,mode,false)==render(fs,mode,true));
 auto e=std::make_unique<dch::Engine>();e->setMode(dch::kDream);e->prepare(48000);{NoAllocations guard;e->reset();}
 // Dream tail must decay to silence without a sustained noise floor, with ducking both off/on.
 for(double duck:{0.,1.}){e->reset();double early=0,late=0;for(int i=0;i<48000*12;++i){float l=i<48000?float(.08*sin(i*.07)):0,r=l;
 {NoAllocations guard;e->process(&l,&r,1,1,1,1,1,duck);}CHECK(std::isfinite(l)&&std::isfinite(r));if(i>48000&&i<96000)early+=double(l)*l;if(i>48000*11)late+=double(l)*l;}
 CHECK(early>0);CHECK(late<early*1e-5);}
 float l=std::numeric_limits<float>::quiet_NaN(),r=std::numeric_limits<float>::infinity();e->process(&l,&r,1,l,l,l,l,l);CHECK(std::isfinite(l)&&std::isfinite(r));
 puts("Dreamcatcher: all modes/rates, block partition, ducking tails, reset/process allocations and finite controls passed");}
