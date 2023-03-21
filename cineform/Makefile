include config
param = -O3 -msse2 -I$(include) -I$(root)/codec -I$(root)/common -I$(root)/convertlib -D_WINDOWS -D_ASMOPT=0

common = MessageQueue.o ThreadMessage.o ThreadPool.o Settings.o
decoder = CFHDDecoder.o CFHDMetadata.o Conversion.o ISampleDecoder.o SampleDecoder.o SampleMetadata.o
encoder = AsyncEncoder.o CFHDEncoder.o CFHDEncoderMetadata.o CFHDEncoderPool.o EncoderPool.o EncoderQueue.o MetadataWriter.o SampleEncoder.o VideoBuffers.o
convertlib = Bilinear.o ColorMatrix.o ConvertYUV8.o DPXConverter.o ImageConverter.o ImageScaler.o MemAlloc.o
codec = DemoasicFrames.o exception.o lutpath.o allocator.o bayer.o bitstream.o buffer.o codebooks.o codec.o convert.o cpuid.o debug.o decoder.o dither.o encoder.o entropy_threading.o filter.o frame.o image.o InvertHorizontalStrip16s.o keyframes.o metadata.o quantize.o recursive.o RGB2YUV.o scanner.o spatial.o stats.o swap.o temporal.o thread.o thumbnail.o timing.o vlc.o wavelet.o 
objects = $(common) $(decoder) $(encoder) $(convertlib) $(codec)

make: $(objects)
	ar rcs libcfhd.a $(objects)

MessageQueue.o: $(root)/Common/MessageQueue.cpp
	gcc -c $(param) -o $@ $<

Settings.o: $(root)/Common/Settings.cpp
	gcc -c $(param) -o $@ $<

ThreadMessage.o: $(root)/Common/ThreadMessage.cpp
	gcc -c $(param) -o $@ $<

ThreadPool.o: $(root)/Common/ThreadPool.cpp
	gcc -c $(param) -o $@ $<

CFHDDecoder.o: $(root)/DecoderSDK/CFHDDecoder.cpp
	gcc -c $(param) -o $@ $<

CFHDMetadata.o: $(root)/DecoderSDK/CFHDMetadata.cpp
	gcc -c $(param) -o $@ $<

Conversion.o: $(root)/DecoderSDK/Conversion.cpp
	gcc -c $(param) -o $@ $<

ISampleDecoder.o: $(root)/DecoderSDK/ISampleDecoder.cpp
	gcc -c $(param) -o $@ $<

SampleDecoder.o: $(root)/DecoderSDK/SampleDecoder.cpp
	gcc -c $(param) -o $@ $<

SampleMetadata.o: $(root)/DecoderSDK/SampleMetadata.cpp
	gcc -c $(param) -o $@ $<

AsyncEncoder.o: $(root)/EncoderSDK/AsyncEncoder.cpp
	gcc -c $(param) -o $@ $<

CFHDEncoder.o: $(root)/EncoderSDK/CFHDEncoder.cpp
	gcc -c $(param) -o $@ $<

CFHDEncoderMetadata.o: $(root)/EncoderSDK/CFHDEncoderMetadata.cpp
	gcc -c $(param) -o $@ $<

CFHDEncoderPool.o: $(root)/EncoderSDK/CFHDEncoderPool.cpp
	gcc -c $(param) -o $@ $<

EncoderPool.o: $(root)/EncoderSDK/EncoderPool.cpp
	gcc -c $(param) -o $@ $<

EncoderQueue.o: $(root)/EncoderSDK/EncoderQueue.cpp
	gcc -c $(param) -o $@ $<

MetadataWriter.o: $(root)/EncoderSDK/MetadataWriter.cpp
	gcc -c $(param) -o $@ $<

SampleEncoder.o: $(root)/EncoderSDK/SampleEncoder.cpp
	gcc -c $(param) -o $@ $<

VideoBuffers.o: $(root)/EncoderSDK/VideoBuffers.cpp
	gcc -c $(param) -o $@ $<

Bilinear.o: $(root)/ConvertLib/Bilinear.cpp
	gcc -c $(param) -o $@ $<

ColorMatrix.o: $(root)/ConvertLib/ColorMatrix.cpp
	gcc -c $(param) -o $@ $<

ConvertYUV8.o: $(root)/ConvertLib/ConvertYUV8.cpp
	gcc -c $(param) -o $@ $<

DPXConverter.o: $(root)/ConvertLib/DPXConverter.cpp
	gcc -c $(param) -o $@ $<

ImageConverter.o: $(root)/ConvertLib/ImageConverter.cpp
	gcc -c $(param) -o $@ $<

ImageScaler.o: $(root)/ConvertLib/ImageScaler.cpp
	gcc -c $(param) -o $@ $<

MemAlloc.o: $(root)/ConvertLib/MemAlloc.cpp
	gcc -c $(param) -o $@ $<

exception.o: $(root)/Codec/exception.cpp
	gcc -c $(param) -o $@ $<

lutpath.o: $(root)/Codec/lutpath.cpp
	gcc -c $(param) -o $@ $<

DemoasicFrames.o: $(root)/Codec/DemoasicFrames.cpp
	gcc -c $(param) -o $@ $<

allocator.o: $(root)/Codec/allocator.c
	gcc -c $(param) -o $@ $<

bandfile.o: $(root)/Codec/bandfile.c
	gcc -c $(param) -o $@ $<

bayer.o: $(root)/Codec/bayer.c
	gcc -c $(param) -o $@ $<

bitstream.o: $(root)/Codec/bitstream.c
	gcc -c $(param) -o $@ $<

buffer.o: $(root)/Codec/buffer.c
	gcc -c $(param) -o $@ $<

codebooks.o: $(root)/Codec/codebooks.c
	gcc -c $(param) -o $@ $<

codec.o: $(root)/Codec/codec.c
	gcc -c $(param) -o $@ $<

convert.o: $(root)/Codec/convert.c
	gcc -c $(param) -o $@ $<

cpuid.o: $(root)/Codec/cpuid.c
	gcc -c $(param) -o $@ $<

debug.o: $(root)/Codec/debug.c
	gcc -c $(param) -o $@ $<

decoder.o: $(root)/Codec/decoder.c
	gcc -c $(param) -o $@ $<

dither.o: $(root)/Codec/dither.c
	gcc -c $(param) -o $@ $<

draw.o: $(root)/Codec/draw.c
	gcc -c $(param) -o $@ $<

dump.o: $(root)/Codec/dump.c
	gcc -c $(param) -o $@ $<

encoder.o: $(root)/Codec/encoder.c
	gcc -c $(param) -o $@ $<

entropy_threading.o: $(root)/Codec/entropy_threading.c
	gcc -c $(param) -o $@ $<

filter.o: $(root)/Codec/filter.c
	gcc -c $(param) -o $@ $<

frame.o: $(root)/Codec/frame.c
	gcc -c $(param) -o $@ $<

image.o: $(root)/Codec/image.c
	gcc -c $(param) -o $@ $<

InvertHorizontalStrip16s.o: $(root)/Codec/InvertHorizontalStrip16s.c
	gcc -c $(param) -o $@ $<

keyframes.o: $(root)/Codec/keyframes.c
	gcc -c $(param) -o $@ $<

metadata.o: $(root)/Codec/metadata.c
	gcc -c $(param) -o $@ $<

quantize.o: $(root)/Codec/quantize.c
	gcc -c $(param) -o $@ $<

recursive.o: $(root)/Codec/recursive.c
	gcc -c $(param) -o $@ $<

RGB2YUV.o: $(root)/Codec/RGB2YUV.c
	gcc -c $(param) -o $@ $<

scanner.o: $(root)/Codec/scanner.c
	gcc -c $(param) -o $@ $<

spatial.o: $(root)/Codec/spatial.c
	gcc -c $(param) -o $@ $<

stats.o: $(root)/Codec/stats.c
	gcc -c $(param) -o $@ $<

swap.o: $(root)/Codec/swap.c
	gcc -c $(param) -o $@ $<

temporal.o: $(root)/Codec/temporal.c
	gcc -c $(param) -o $@ $<

thread.o: $(root)/Codec/thread.c
	gcc -c $(param) -o $@ $<

thumbnail.o: $(root)/Codec/thumbnail.c
	gcc -c $(param) -o $@ $<

timing.o: $(root)/Codec/timing.c
	gcc -c $(param) -o $@ $<

vlc.o: $(root)/Codec/vlc.c
	gcc -c $(param) -o $@ $<

wavelet.o: $(root)/Codec/wavelet.c
	gcc -c $(param) -o $@ $<
