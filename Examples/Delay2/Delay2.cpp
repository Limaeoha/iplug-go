#include "Delay2.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "gen/proto/samples.pb.h"

#include <stdio.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



#define READ 0
#define WRITE 1

pid_t popen2(const char *command, int *infp, int *outfp) {
  int p_stdin[2], p_stdout[2];
  pid_t pid;

  if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
    return -1;

  pid = fork();
  if (pid < 0)
    return pid;
  else if (pid == 0)
  {
    close(p_stdin[WRITE]);
    dup2(p_stdin[READ], READ);
    close(p_stdout[READ]);
    dup2(p_stdout[WRITE], WRITE);
    execl("/bin/sh", "sh", "-c", command, NULL);
    perror("execl");
    exit(1);
  }

  if (infp == NULL)
    close(p_stdin[WRITE]);
  else
    *infp = p_stdin[WRITE];
  if (outfp == NULL)
    close(p_stdout[READ]);
  else
    *outfp = p_stdout[READ];
  return pid;
}

int infp, outfp;
char *buf = (char*)calloc(128, sizeof(char));

int startServer() {
  int pid =  popen2("/Users/ianlozinski/code/iPlug2/Examples/Delay2/echo/echo", &infp, &outfp);
  if (pid != 0) {
    printf("child pid: %d\n", pid);
  }
  printf("parent");
  return 0;
}



Delay2::Delay2(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
//  GetParam(kGain)->InitDouble("Gain", 0., 0., 100.0, 0.01, "%");
  GetParam(kPan) -> InitDouble("Pan", 0., -1, 1, 0.01, "%");
  startServer();


#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
//    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    IGraphicsMac* pGraphics = new IGraphicsMac(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
      pGraphics->SetBundleID(BUNDLE_ID);
      pGraphics->SetSharedResourcesSubPath(SHARED_RESOURCES_SUBPATH);
      
      return pGraphics;
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_INDIGO);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IRECT b = pGraphics->GetBounds();
    pGraphics->AttachControl(new ITextControl(b.GetMidVPadded(50), "Hello iPlug 2!", IText(50)));
    pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetVShifted(100), kPan));
  };
  
#endif
}


void addChannel(sample* samples, int nframes, samples::SamplePacket_Channel *chan) {
  for (int i = 0; i < nframes; i++) {
    chan->add_samples(samples[i]);
  }
}

void* buffer = malloc(100000);

samples::SamplePacket packet;

#if IPLUG_DSP
void Delay2::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // build message
  packet.Clear();
  packet.set_nchans(NOutChansConnected());
  packet.set_nframes(nFrames);
  packet.set_samplerate(GetSampleRate());
  for (int c = 0; c < packet.nchans(); c++) {
    addChannel(inputs[c], nFrames, packet.add_channels());
  }

  // write size of outgoing message
  size_t messageSize = packet.ByteSizeLong();
  write(infp, &messageSize, sizeof(messageSize));
  
 
  // write outoging message
  packet.SerializeToFileDescriptor(infp);
  packet.Clear();

  // read incoming message size
  read(outfp, &messageSize, sizeof(messageSize));
  
  // read incoming message
  read(outfp, buffer, messageSize);
  packet.ParseFromArray(buffer, messageSize);

  for (int c = 0; c < packet.channels_size(); c++) {
    samples::SamplePacket_Channel chan = packet.channels(c);
    for (int s = 0; s < chan.samples_size(); s++) {
      outputs[c][s] = chan.samples(s);
    }
  }
}

#endif
