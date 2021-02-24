#include "Stethoscope.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "gen/proto/samples.pb.h"

#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#define READ 0
#define WRITE 1

pid_t popen2(const char *command, char * dir) {
  pid_t pid;
  
  printf("forking\n");

  pid = fork();
  if (pid < 0) {
    printf("unable to fork\n");
    return pid;
  }

  // pid of 0 means child
  if (pid == 0) {
    printf("child\n");
//    execl("/bin/sh", "sh", "-c", command, dir, NULL);
    execl(command, dir, NULL);
    throw "couldn't start child process";
  }
  printf("parent\n");
  return pid;
}

int sendfp, returnfp;
char *buf = (char*)calloc(128, sizeof(char));

std::ifstream returnS;
std::ofstream sendS;


int startServer() {
  char dir[15];
  strcpy(dir, "/tmp/delay.XXXXXX");

  mkdtemp(dir);
  
  if (mkfifo(String(dir) + "/send", 0666) != 0) {
    std::cout << "couldn't create fifo";
    return 1;
  }
  
  if (mkfifo(String(dir) + "/return", 0666) != 0) {
    std::cout << "couldn't create fifo";
    return 1;
  }
  
  std::cout << dir;
  std::cout << "\ncreated the fifos probably\n";
  
  sendS.open(String(dir) + "/send", std::ofstream::out);
  if (!sendS.is_open() ){
      std::cout << " error : cannot open file " << std :: endl;
      return 1;
  }
  
  returnS.open(String(dir) + "/return", std::ifstream::in);
  if (!returnS.is_open()) {
    std::cout << " error : cannot open file " << std :: endl;
        return 1;
  }
  
//  printf("opening send fifo...");
//  sendfp = open(String(dir) + "/send", O_WRONLY );
//  printf("ok\n");
//  printf("opening return fifo...");
//  returnfp = open(String(dir) + "/return", O_RDONLY);
//  printf("ok\n");
  return popen2("/Users/ianlozinski/code/iPlug2/Examples/Stethoscope/echo/echo", dir);
}



Stethoscope::Stethoscope(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
//  GetParam(kGain)->InitDouble("Gain", 0., 0., 100.0, 0.01, "%");
  GetParam(kPan) -> InitDouble("Pan", 0., -1, 1, 0.01, "%");
  startServer();

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
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
void Stethoscope::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
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
  write(sendfp, &messageSize, sizeof(messageSize));
  
  // write outoging message
  packet.SerializeToFileDescriptor(sendfp);
  packet.Clear();

  // read incoming message size
  read(returnfp, &messageSize, sizeof(messageSize));
  
  // read incoming message
  read(returnfp, buffer, messageSize);
  packet.ParseFromArray(buffer, messageSize);

  for (int c = 0; c < packet.channels_size(); c++) {
    samples::SamplePacket_Channel chan = packet.channels(c);
    for (int s = 0; s < chan.samples_size(); s++) {
      outputs[c][s] = chan.samples(s);
    }
  }
}

#endif
