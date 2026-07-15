using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace MakeReadSample;

class MDLSampleCompressor
{
	public static byte[] Compress(sbyte[] sampleData)
	{
		var output = new MemoryStream();

		Compress(MemoryMarshal.Cast<sbyte, byte>(sampleData.AsSpan()), hasLowByte: false, output);

		return output.ToArray();
	}

	public static byte[] Compress(short[] sampleData)
	{
		var output = new MemoryStream();

		Compress(MemoryMarshal.Cast<short, byte>(sampleData.AsSpan()), hasLowByte: true, output);

		return output.ToArray();
	}

	public static void Compress(sbyte[] sampleData, Stream output)
	{
		Compress(MemoryMarshal.Cast<sbyte, byte>(sampleData.AsSpan()), hasLowByte: false, output);
	}

	public static void Compress(short[] sampleData, Stream output)
	{
		Compress(MemoryMarshal.Cast<short, byte>(sampleData.AsSpan()), hasLowByte: true, output);
	}

	static void Compress(Span<byte> sampleData, bool hasLowByte, Stream output)
	{
		// We need to length-prefix the output. Since our output interface is a
		// sequential stream, we need to buffer the whole shebang.

		var buffer = new MemoryStream();

		using (var bitWriter = new BitWriter(buffer))
			DoCompression(sampleData, hasLowByte, bitWriter);

		output.Write(BitConverter.GetBytes((int)buffer.Length));

		buffer.Position = 0;
		buffer.CopyTo(output);
	}

	static void DoCompression(Span<byte> sampleData, bool hasLowByte, BitWriter output)
	{
		byte lastHighByte = 0;

		while (sampleData.Length > 0)
		{
			if (hasLowByte)
			{
				output.Write(sampleData[0], 8);
				sampleData = sampleData.Slice(1);
			}

			int delta = sampleData[0] - lastHighByte;

			lastHighByte = sampleData[0];

			bool sign = (delta < 0);

			if (sign)
				delta = ~delta;

			output.WriteBit(sign);

			if (delta < 8)
			{
				output.WriteBit(true);
				output.Write(delta, 3);
			}
			else
			{
				output.WriteBit(false);

				delta -= 8;

				while (delta > 15)
				{
					output.WriteBit(false);
					delta -= 16;
				}

				output.WriteBit(true);
				output.Write(delta, 4);
			}

			if (sign)
				delta = ~delta;

			sampleData = sampleData.Slice(1);
		}
	}
}
