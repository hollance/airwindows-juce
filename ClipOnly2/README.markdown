# ClipOnly2

ClipOnly2 suppresses the brightness of digital clipping without affecting unclipped samples, at any sample rate.

Video: https://www.airwindows.com/cliponly2/

Description from [Airwindowpedia.txt](https://github.com/airwindows/airwindows/blob/master/Airwindopedia.txt):

> ClipOnly2 is the heart of my mastering-grade clipping algorithm. Instead of trying to define the cleanest possible nasty sharp edge, or doing a soft-clip thing, ClipOnly passes through ALL nonclipped samples totally untouched… but when you get a clipped sample, what ClipOnly does is it takes the sample entering clipping, and the sample exiting clipping, and it interpolates between the last unclipped sample and the clipped stuff. So, it is synthesizing a soft entry and exit from what is otherwise total hard clipping, and if only the one sample clipped? That very bright clip simply goes away, turned right down.
>
> This produces a hard-clip suitable for safety clipper purposes, which is purely ‘bypass’ (plus a one sample delay to allow for the processing), with softer highs than you’d get from any pure hard-clip, no matter how oversampled. It’s an alternate technique, and is also pretty CPU-efficient.
>
> ClipOnly2 takes this principle and changes the ‘one sample’ to ‘the space of one sample at 44.1k’. Same tone, same ear-friendly approach to clipping extreme highs, except that now it’s effective at high sample rates. I’m demonstrating it and its predecessor at 96k, but ClipOnly2 is designed to work up to 700k or so, in case people get giddy with their newfound power :)
