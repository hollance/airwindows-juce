# BitShiftGain

BitShiftGain is the ‘One Weird Trick’ perfect boost/pad, but in 6db increments ONLY.

Video: https://www.airwindows.com/bitshiftgain/

Description from [Airwindowpedia.txt](https://github.com/airwindows/airwindows/blob/master/Airwindopedia.txt):

> Here is one final trick for clean gain aficionados.
>
> Turns out the only way to get cleaner gain trim than PurestGain, with its high mathematical precision and noise shaping… is not to do any of that. No fancy math, no noise shaping or dither. Just a very narrowly defined boost or cut, in the form of a ‘bit shift’.
>
> Doing this means your waveform is scaled up or down by increments of 6 dB exactly. No 3 db, no 9, no 7 or even 6.001! Only 6 or 12 or 18 and so on, up or down. Select the number of bits you want to shift, and BitShiftGain applies the exact number, not even calculating it in floating-point through repeated operations: from a look-up table to make sure it’s absolutely exact and precise.
>
> And when it does, all the bits shift neatly to the side inside your audio, and whether you lose the smallest and subtlest or gain up and fill it in with a zero… every single sample in your audio is in exactly, EXACTLY the same relative position to the others. Apart from the gain or loss of the smallest bit, there is literally no change to the audio at all: if there was a noise shaping, it would have nothing to work with.
>
> Perfection, at exclusively increments of 6 dB. That’s the catch. You probably can’t mix with gain changes that coarse (though it’s tempting to try!) but here’s what you can do: you can take 24-bit dithers, gain down 8 bits in front and 8 bits up after, and have a perfect 16 bit dither. Or a 17 bit, if that pleases you… or shift 16 bits down so you can hear what your dither’s noise floor acts like (we’ll be doing lots of that when I start bringing out the dithers). +-16 bits of gain trim is a very big boost or cut. The overall range of BitShiftGain is huge. But the real magic of BitShiftGain is the sheer simplicity of the concept. Provided your math is truly, rigorously accurate and your implementation’s perfect, gain trim with bit shift is the only way in digital (fixed OR floating point) where you can apply a change, and the word length of your audio doesn’t have to expand, AND every sample which remains in your audio continues to be in exactly the same relation to all the others.
>
> Digital audio is like some crystalline structure: it’s fragile, brittle, and suffers tiny fractures at the tiniest alterations. There’s almost nothing you can do in digital audio that’s not going to cause some damage. But as long as you stick to 6 dB steps and rigidly control the implementation (BitShiftGain doesn’t even store the audio in a temporary variable!), you can chip away at that least significant bit, and the whole minutes-or-hours-long crystalline structure of digital bits can remain perfectly intact above it.
