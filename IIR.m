% LPF
% fc = 240 Hz
% fs = 1200 Hz
% f = 200 Hz
% N = 5

close all; clear; clc;
fc = 240;
fs = 1200;
f = 200;
Ts = 1/fs; 
t = 0:Ts:0.1; 
X = sin(2*pi*f*t);

[b,a] = butter(5,fc/(fs/2),'low');
freqz(b,a,[],fs)
subplot(2,1,1)
ylim([-100 20])
Y = filter(b,a,X);
X
