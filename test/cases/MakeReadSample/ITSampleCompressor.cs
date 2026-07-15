using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace MakeReadSample;

public class ITSampleCompressor
{
	public static byte[] GetCompressedBytes(sbyte[] mono, bool doubleDelta)
	{
		var buffer = new MemoryStream();

		Compress8(mono, doubleDelta, buffer);

		return buffer.ToArray();
	}

	public static byte[] GetCompressedBytes(sbyte[] left, sbyte[] right, bool doubleDelta)
	{
		var buffer = new MemoryStream();

		Compress8(left, doubleDelta, buffer);
		Compress8(right, doubleDelta, buffer);

		return buffer.ToArray();
	}

	public static byte[] GetCompressedBytes(short[] mono, bool doubleDelta)
	{
		var buffer = new MemoryStream();

		Compress16(mono, doubleDelta, buffer);

		return buffer.ToArray();
	}

	public static byte[] GetCompressedBytes(short[] left, short[] right, bool doubleDelta)
	{
		var buffer = new MemoryStream();

		Compress16(left, doubleDelta, buffer);
		Compress16(right, doubleDelta, buffer);

		return buffer.ToArray();
	}

	// Needs to be called twice for stereo samples, once for left and once for right.
	public static void Compress8(sbyte[] samples, bool doubleDelta, Stream output)
	{
		Span<short> samples16 = stackalloc short[samples.Length];

		for (int i = 0; i < samples.Length; i++)
			samples16[i] = samples[i];

		Compress(samples16, 8, doubleDelta, output);
	}

	// Needs to be called twice for stereo samples, once for left and once for right.
	public static void Compress16(short[] samples, bool doubleDelta, Stream output)
	{
		// Make a copy because we're going to overwrite it with the deltas.
		Span<short> samples16 = stackalloc short[samples.Length];

		samples.CopyTo(samples16);

		Compress(samples16, 16, doubleDelta, output);
	}

	struct QueuedSample
	{
		public int Bits;
		public int SampleValue;

		public override string ToString() => Bits + ": " + SampleValue;
	}

	public static void Compress(Span<short> samples, int sampleBitWidth, bool doubleDelta, Stream output)
	{
		const int BlockSize = 32768;

		int samplesPerBlock = BlockSize * 8 / sampleBitWidth;

		int maximumBitWidth = sampleBitWidth + 1;
		int sampleMask = (1 << sampleBitWidth) - 1;

		int bitWidthWidth = 0;

		for (int i = maximumBitWidth - 1; i > 1; i >>= 1)
			bitWidthWidth++;

		// Turn samples into deltas.
		for (int pass = doubleDelta ? 0 : 1; pass < 2; pass++)
		{
			for (int i = samples.Length - 1; i > 0; i--)
				samples[i] -= samples[i - 1];
		}

		if (sampleBitWidth == 8)
		{
			for (int i = 0; i < samples.Length; i++)
				samples[i] = unchecked((sbyte)(samples[i] & 0xFF));
		}

		var blockSamples = new short[samplesPerBlock];

		int samplesRemaining = samples.Length;

		using (var outputWriter = new BinaryWriter(output, Encoding.UTF8, leaveOpen: true))
		{
			var queuedSamples = new List<QueuedSample>();

			while (samplesRemaining > 0)
			{
				int thisBlockSamples = Math.Min(samplesRemaining, samplesPerBlock);

				samples.Slice(0, thisBlockSamples).CopyTo(blockSamples);
				samples = samples.Slice(thisBlockSamples);

				Span<short> blockSamplesSpan = blockSamples.AsSpan().Slice(0, thisBlockSamples);

				using (var blockBuffer = new MemoryStream())
				using (var blockWriter = new BitWriter(blockBuffer))
				{
					queuedSamples.Clear();

					int bitWidth = maximumBitWidth;

					void EmitBitWidthChange(int newBitWidth)
					{
						if (bitWidth <= 6)
						{
							int semaphore = 1 << (bitWidth - 1);

							// We have to fit 9 possible values into 3 bits, or 17 possible values into 4 bits.
							// Fortunately, we can rule out one of the values: the current bit width we're
							// switching away from. So, if the new bit width is larger than it, we can shift
							// everything down one, and then reverse the shift when reading, which works
							// because we never need to transition to the current bit width.

							int compressedBitWidth = newBitWidth;

							if (compressedBitWidth > bitWidth)
								compressedBitWidth--;

							compressedBitWidth--;

							blockWriter.Write(semaphore, bitWidth);
							blockWriter.Write(compressedBitWidth, bitWidthWidth);
						}
						else if (bitWidth < maximumBitWidth)
						{
							int field = 0xFFFF >> (17 - bitWidth);

							int border = field - (1 << (bitWidthWidth - 1));

							// We have to fit 9 possible values into 3 bits, or 17 possible values into 4 bits.
							// Fortunately, we can rule out one of the values: the current bit width we're
							// switching away from. So, if the new bit width is larger than it, we can shift
							// everything down one, and then reverse the shift when reading, which works
							// because we never need to transition to the current bit width.

							int compressedBitWidth = newBitWidth;

							if (compressedBitWidth > bitWidth)
								compressedBitWidth--;

							blockWriter.Write(border + compressedBitWidth, bitWidth);
						}
						else
							blockWriter.Write((1 << (1 << bitWidthWidth)) | (newBitWidth - 1), maximumBitWidth);

						bitWidth = newBitWidth;
					}

					int GetBitWidthChangeCost(int fromBitWidth, int toBitWidth)
					{
						if (toBitWidth == fromBitWidth)
							return 0;

						if (fromBitWidth <= 6)
						{
							int semaphoreBits = bitWidth;

							return semaphoreBits + bitWidthWidth;
						}
						else
							return fromBitWidth;
					}

					int GetBitsRequiredForSample(short sample)
					{
						short test = sample;

						for (int bits = 1; bits <= 17; bits++)
						{
							if (test == 0)
							{
								if ((bits > 6) && (bits < maximumBitWidth))
								{
									// The border values are stolen for switches to other bit widths.
									// Half of the possible new bit width values are at the top end
									// of the range, and the other half are at the bottom end of the
									// range.
									int bitWidthValueRange = 1 << bitWidthWidth;

									int maximumExpressibleValue = (1 << (bits - 1)) - (bitWidthValueRange >> 1) - 1;
									int minimumExpressibleValue = -maximumExpressibleValue - 1;

									if ((sample < minimumExpressibleValue) || (sample > maximumExpressibleValue))
										bits++;
								}

								return bits;
							}

							test /= 2;
						}

						return maximumBitWidth;
					}

					void PackSubsequences(Span<QueuedSample> samples)
					{
						// Find each transition. If it is an increase in bit width,
						// check whether it would be cheaper to simply switch to the
						// greater bit width (and thus only switch width once). If
						// it is a decrease in bit width, check whether it would be
						// cheaper to simply stay on the preceding bit width.

						int spanStart = 0;

						while (spanStart < samples.Length)
						{
							int spanBitWidth = samples[spanStart].Bits;
							int precedingSpanBitWidth = maximumBitWidth;

							int spanEnd = spanStart + 1;

							while (spanEnd < samples.Length)
							{
								int nextSpanBitWidth = samples[spanEnd].Bits;

								if (nextSpanBitWidth > spanBitWidth)
								{
									// Increase
									int spanLength = spanEnd - spanStart;

									int costToSwitchToShorterSamplesForSpan =
										GetBitWidthChangeCost(precedingSpanBitWidth, spanBitWidth) +
										spanLength * spanBitWidth +
										GetBitWidthChangeCost(spanBitWidth, nextSpanBitWidth);

									int costToUseWiderSamplesButAvoidWidthChange =
										GetBitWidthChangeCost(precedingSpanBitWidth, nextSpanBitWidth) +
										spanLength * nextSpanBitWidth;

									if (costToUseWiderSamplesButAvoidWidthChange < costToSwitchToShorterSamplesForSpan)
									{
										for (int i = spanStart; i < spanEnd; i++)
											samples[i].Bits = nextSpanBitWidth;
									}
									else
										spanStart = spanEnd;

									spanBitWidth = nextSpanBitWidth;
								}
								else if (nextSpanBitWidth < spanBitWidth)
								{
									// Decrease -- find end of the next span
									int nextSpanEnd = spanEnd;

									while ((nextSpanEnd < samples.Length) && (samples[nextSpanEnd].Bits == nextSpanBitWidth))
										nextSpanEnd++;

									int spanLength = spanEnd - spanStart;
									int nextSpanLength = nextSpanEnd - spanEnd;
									int combinedSpanLength = nextSpanEnd - spanStart;

									int nextSpanWidthChangeCost = (nextSpanEnd + 1 < samples.Length)
										? GetBitWidthChangeCost(nextSpanBitWidth, -1)
										: 0;

									int costToSwitchToShorterSamplesForNextSpan =
										spanLength * bitWidth +
										GetBitWidthChangeCost(spanBitWidth, nextSpanBitWidth) +
										nextSpanLength * nextSpanBitWidth +
										nextSpanWidthChangeCost;

									int costToMergeNextSpanIntoThisOneAndAvoidWidthChange =
										combinedSpanLength * bitWidth +
										GetBitWidthChangeCost(spanBitWidth, -1);

									if (costToMergeNextSpanIntoThisOneAndAvoidWidthChange < costToSwitchToShorterSamplesForNextSpan)
									{
										while (spanEnd < nextSpanEnd)
										{
											samples[spanEnd].Bits = spanBitWidth;
											spanEnd++;
										}
									}
									else
									{
										spanStart = spanEnd;
										spanBitWidth = nextSpanBitWidth;
									}

									if (nextSpanEnd >= samples.Length)
										return;

									// Reprocess next transition
									spanEnd--;
								}

								spanEnd++;
							}

							if (spanEnd >= samples.Length)
								return;
						}
					}

					while (blockSamplesSpan.Length > 0)
					{
						short sample = blockSamplesSpan[0];

						QueuedSample queuedSample;

						queuedSample.SampleValue = sample;
						queuedSample.Bits = GetBitsRequiredForSample(sample);

						if (queuedSample.Bits < maximumBitWidth)
							queuedSamples.Add(queuedSample);
						else
						{
							PackSubsequences(CollectionsMarshal.AsSpan(queuedSamples));

							for (int i = 0; i < queuedSamples.Count; i++)
							{
								var s = queuedSamples[i];

								if (s.Bits != bitWidth)
									EmitBitWidthChange(s.Bits);

								blockWriter.Write(s.SampleValue, s.Bits);
							}

							queuedSamples.Clear();

							if (bitWidth < maximumBitWidth)
								EmitBitWidthChange(maximumBitWidth);

							blockWriter.Write(queuedSample.SampleValue & sampleMask, queuedSample.Bits);
						}

						blockSamplesSpan = blockSamplesSpan.Slice(1);
					}

					if (queuedSamples.Count > 0)
					{
						PackSubsequences(CollectionsMarshal.AsSpan(queuedSamples));

						for (int i = 0; i < queuedSamples.Count; i++)
						{
							var s = queuedSamples[i];

							if (s.Bits != bitWidth)
								EmitBitWidthChange(s.Bits);

							blockWriter.Write(s.SampleValue & sampleMask, s.Bits);
						}
					}

					blockWriter.Close();

					outputWriter.Write((ushort)blockWriter.BytesWritten);
					outputWriter.Write(blockBuffer.GetBuffer().AsSpan().Slice(0, (int)blockBuffer.Length));
				}

				samplesRemaining -= thisBlockSamples;
			}
		}
	}
}
