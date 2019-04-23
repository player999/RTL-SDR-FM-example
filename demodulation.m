%Found 1 device(s):
%  0:  Realtek, RTL2838UHIDIR, SN: 00000001
%
%Using device 0: Generic RTL2832U OEM
%Detached kernel driver
%Found Rafael Micro R820T tuner
%Tuner gain set to automatic.
%Tuned to 100252000 Hz.
%Oversampling input by: 8x.
%Oversampling output by: 1x.
%Buffer size: 8.13ms
%Exact sample rate is: 1008000.009613 Hz
%Sampling at 1008000 S/s.
%Output at 126000 Hz.
%Playing raw data 'stdin' : Signed 16 bit Little Endian, Rate 126000 Hz, Mono
%underrun!!! (at least 128,219 ms long)

f = fopen('dump.bin', 'r');
Fs = 1008000;
AFs = 44100;
raw_values = fread(f,'uint8');
fclose(f);
mat = double(raw_values)-127;
Q = mat(2:2:end);
I = mat(1:2:end);
y = diff(atan2(Q,I));
y(y > pi) = y(y > pi) - 2*pi;
y(y < -pi) = y(y < -pi) + 2*pi;
z = movmean(y, 30);
center = mean(z);
z = z - center;
z = downsample(z, round(Fs/AFs));
sound(z, AFs);

