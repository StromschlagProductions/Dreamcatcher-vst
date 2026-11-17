#include "PluginProcessor.h"
#include "TestSupport.h"
#include <memory>
int main(){juce::ScopedJuceInitialiser_GUI init;auto p=std::make_unique<DreamcatcherProcessor>();p->prepareToPlay(48000,64);juce::MidiBuffer midi;juce::AudioBuffer<float>b(2,64);
 for(int mode=0;mode<4;++mode){auto* v=p->apvts.getParameter("mode");v->setValueNotifyingHost(v->convertTo0to1(float(mode)));b.clear();b.setSample(0,0,.1f);{NoAllocations guard;p->processBlock(b,midi);p->reset();}for(int c=0;c<2;++c)for(int i=0;i<64;++i)CHECK(std::isfinite(b.getSample(c,i)));
}
 // Exercise a real JUCE message loop: latency is published outside the audio callback.
 juce::Timer::callAfterDelay(100,[&]{
   CHECK(p->getLatencySamples()==int(48000*dch::kGhostSeconds)+dch::SwellConvolver::B);
   auto* mode=p->apvts.getParameter("mode");mode->setValueNotifyingHost(mode->convertTo0to1(0));b.clear();
   {NoAllocations guard;p->processBlock(b,midi);}
   juce::Timer::callAfterDelay(100,[&]{CHECK(p->getLatencySamples()==0);juce::MessageManager::getInstance()->stopDispatchLoop();});
 });
 juce::MessageManager::getInstance()->runDispatchLoop();

 auto* duck=p->apvts.getParameter("duck");duck->setValueNotifyingHost(.75f);juce::MemoryBlock state;p->getStateInformation(state);duck->setValueNotifyingHost(0);p->setStateInformation(state.getData(),(int)state.getSize());CHECK(std::abs(p->apvts.getRawParameterValue("duck")->load()-.75f)<1e-5f);
 puts("Dreamcatcher processor: reset/callback allocations, mode switching and ducking state roundtrip passed");}
