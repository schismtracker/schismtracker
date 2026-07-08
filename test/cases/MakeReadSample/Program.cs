using System.Buffers.Binary;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace MakeReadSample;

class Program
{
	static void Main()
	{
		using (s_casesWriter = new StreamWriter("../readsample.cases.inc"))
		using (s_indexWriter = new StreamWriter("../../../include/test-funcs.readsample.inc"))
			Generate();
	}

	static TextWriter s_casesWriter = new StringWriter();
	static TextWriter s_indexWriter = new StringWriter();

	static void Generate()
	{
		sbyte[] expected8bitLeft = { 0, 2, 4, 6, 122, 124, 126, -2, -4, -6, -124, -126, -128 };
		sbyte[] expected8bitRight = { 76, 75, 74, 73, 72, 71, 70, -100, -90, -80, -70, -60, -50 };

		sbyte[] expected8bitStereoSplit = expected8bitLeft.Concat(expected8bitRight).ToArray();
		sbyte[] expected8bitStereoInterleaved = expected8bitLeft.Zip(expected8bitRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		int[] expected16bitLeft_i = { 0x0000, 0x0100, 0x0300, 0x0600, 0x0A00, 0x0F00, 0x1FFF, 0x3FFF, 0x7FFF, 0x8001, 0x8008, 0x9010, 0xA100, 0xC000, 0xE000, 0xF000, 0xFFFF };
		int[] expected16bitRight_i = { 0x0800, 0x0600, 0x0400, 0x0200, 0x0100, 0x0010, 0x0001, 0x0000, 0xFFFF, 0xFFF0, 0xFF00, 0xF000, 0xD000, 0xB000, 0x9000, 0x8800, 0x8001 };

		short[] expected16bitLeft = expected16bitLeft_i.Select(x => unchecked((short)x)).ToArray();
		short[] expected16bitRight = expected16bitRight_i.Select(x => unchecked((short)x)).ToArray();

		short[] expected16bitStereoSplit = expected16bitLeft.Concat(expected16bitRight).ToArray();
		short[] expected16bitStereoInterleaved = expected16bitLeft.Zip(expected16bitRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		s_casesWriter.WriteLine("signed char expected_8bit_mono[] = {");
		s_casesWriter.Write('\t');

		for (int i = 0; i < expected8bitLeft.Length; i++)
		{
			if (i > 0) s_casesWriter.Write(", ");
			s_casesWriter.Write(expected8bitLeft[i]);
		}

		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("};");
		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("signed char expected_8bit_stereo[] = {");

		for (int i = 0; i < expected8bitRight.Length; i++)
			s_casesWriter.WriteLine("\t{0,4}, {1,4}{2}", expected8bitLeft[i], expected8bitRight[i], (i + 1 < expected8bitLeft.Length ? "," : ""));

		s_casesWriter.WriteLine("};");
		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("short expected_16bit_mono[] = {");

		s_casesWriter.Write('\t');
		for (int i = 0; i < expected16bitLeft.Length; i++)
		{
			if (i > 0) s_casesWriter.Write(", ");
			s_casesWriter.Write("0x{0:X4}", expected16bitLeft[i]);
		}
		s_casesWriter.WriteLine();

		s_casesWriter.WriteLine("};");
		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("short expected_16bit_stereo[] = {");

		for (int i = 0; i < expected16bitRight.Length; i++)
			s_casesWriter.WriteLine("\t0x{0:X4}, 0x{1:X4}{2}", expected16bitLeft[i], expected16bitRight[i], (i + 1 < expected16bitLeft.Length ? "," : ""));

		s_casesWriter.WriteLine("};");
		s_casesWriter.WriteLine();

		sbyte[] expected7bit = expected8bitLeft.Select(sample => unchecked((sbyte)(sample >> 1))).ToArray();

		OutputTestCase<SByteEmitter, sbyte>("7,M,LE,PCMS", newLineBeforeBytes: false, expected7bit, ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<SByteEmitter, sbyte>("7,M,BE,PCMS", newLineBeforeBytes: false, expected7bit, ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<SByteEmitter, sbyte>("8,M,LE,PCMS", newLineBeforeBytes: false, expected8bitLeft, ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<SByteEmitter, sbyte>("8,M,BE,PCMS", newLineBeforeBytes: false, expected8bitLeft, ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<ByteEmitter, byte>("8,M,LE,PCMU", newLineBeforeBytes: false, expected8bitLeft.MakeUnsigned(), ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<ByteEmitter, byte>("8,M,BE,PCMU", newLineBeforeBytes: false, expected8bitLeft.MakeUnsigned(), ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<SByteEmitter, sbyte>("8,M,LE,PCMD", newLineBeforeBytes: false, expected8bitLeft.MakeDeltaMono(), ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<SByteEmitter, sbyte>("8,M,BE,PCMD", newLineBeforeBytes: false, expected8bitLeft.MakeDeltaMono(), ":d", 1, Array.Empty<int>(), "EXPECT_8BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<SByteEmitter, sbyte>("8,SS,LE,PCMS", newLineBeforeBytes: true, expected8bitStereoSplit, ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SS,BE,PCMS", newLineBeforeBytes: true, expected8bitStereoSplit, ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<ByteEmitter, byte>("8,SS,LE,PCMU", newLineBeforeBytes: true, expected8bitStereoSplit.MakeUnsigned(), ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<ByteEmitter, byte>("8,SS,BE,PCMU", newLineBeforeBytes: true, expected8bitStereoSplit.MakeUnsigned(), ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SS,LE,PCMD", newLineBeforeBytes: true, expected8bitLeft.MakeDeltaMono().Concat(expected8bitRight.MakeDeltaMono()).ToArray(), ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SS,BE,PCMD", newLineBeforeBytes: true, expected8bitLeft.MakeDeltaMono().Concat(expected8bitRight.MakeDeltaMono()).ToArray(), ":d", 1, [expected8bitLeft.Length], "EXPECT_8BIT | EXPECT_STEREO");

		OutputSeparator();

		OutputTestCase<SByteEmitter, sbyte>("8,SI,LE,PCMS", newLineBeforeBytes: true, expected8bitStereoInterleaved, ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SI,BE,PCMS", newLineBeforeBytes: true, expected8bitStereoInterleaved, ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<ByteEmitter, byte>("8,SI,LE,PCMU", newLineBeforeBytes: true, expected8bitStereoInterleaved.MakeUnsigned(), ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<ByteEmitter, byte>("8,SI,BE,PCMU", newLineBeforeBytes: true, expected8bitStereoInterleaved.MakeUnsigned(), ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SI,LE,PCMD", newLineBeforeBytes: true, expected8bitStereoInterleaved.MakeDeltaStereo(), ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<SByteEmitter, sbyte>("8,SI,BE,PCMD", newLineBeforeBytes: true, expected8bitStereoInterleaved.MakeDeltaStereo(), ",4", 1, [2], "EXPECT_8BIT | EXPECT_STEREO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("16,M,LE,PCMS", newLineBeforeBytes: true, expected16bitLeft.ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,BE,PCMS", newLineBeforeBytes: true, expected16bitLeft.ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,LE,PCMU", newLineBeforeBytes: true, expected16bitLeft.MakeUnsigned().ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,BE,PCMU", newLineBeforeBytes: true, expected16bitLeft.MakeUnsigned().ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,LE,PCMD", newLineBeforeBytes: true, expected16bitLeft.MakeDeltaMono().ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,BE,PCMD", newLineBeforeBytes: true, expected16bitLeft.MakeDeltaMono().ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("16,SS,LE,PCMS", newLineBeforeBytes: true, expected16bitStereoSplit.ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,BE,PCMS", newLineBeforeBytes: true, expected16bitStereoSplit.ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,LE,PCMU", newLineBeforeBytes: true, expected16bitStereoSplit.MakeUnsigned().ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,BE,PCMU", newLineBeforeBytes: true, expected16bitStereoSplit.MakeUnsigned().ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,LE,PCMD", newLineBeforeBytes: true, expected16bitLeft.MakeDeltaMono().Concat(expected16bitRight.MakeDeltaMono()).ToArray().ToBytesLE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,BE,PCMD", newLineBeforeBytes: true, expected16bitLeft.MakeDeltaMono().Concat(expected16bitRight.MakeDeltaMono()).ToArray().ToBytesBE(), "", 2, [18, 16], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("16,SI,LE,PCMS", newLineBeforeBytes: true, expected16bitStereoInterleaved.ToBytesLE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SI,BE,PCMS", newLineBeforeBytes: true, expected16bitStereoInterleaved.ToBytesBE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SI,LE,PCMU", newLineBeforeBytes: true, expected16bitStereoInterleaved.MakeUnsigned().ToBytesLE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SI,BE,PCMU", newLineBeforeBytes: true, expected16bitStereoInterleaved.MakeUnsigned().ToBytesBE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SI,LE,PCMD", newLineBeforeBytes: true, expected16bitStereoInterleaved.MakeDeltaStereo().ToBytesLE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SI,BE,PCMD", newLineBeforeBytes: true, expected16bitStereoInterleaved.MakeDeltaStereo().ToBytesBE(), ",4", 2, [4], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		int[] input24bitLeft = Make24bit(expected16bitLeft);
		int[] input24bitRight = Make24bit(expected16bitRight);

		int[] input24bitStereoInterleaved = input24bitLeft.Zip(input24bitRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		OutputTestCase<HexByteEmitter, byte>("24,M,LE,PCMS", newLineBeforeBytes: true, input24bitLeft.To24bitBytesLE(), "", 3, [18, 18, 15], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("24,M,BE,PCMS", newLineBeforeBytes: true, input24bitLeft.To24bitBytesBE(), "", 3, [18, 18, 15], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("24,M,LE,PCMU", newLineBeforeBytes: true, input24bitLeft.Make24bitUnsigned().To24bitBytesLE(), "", 3, [18, 18, 15], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("24,M,BE,PCMU", newLineBeforeBytes: true, input24bitLeft.Make24bitUnsigned().To24bitBytesBE(), "", 3, [18, 18, 15], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("24,SI,LE,PCMS", newLineBeforeBytes: true, input24bitStereoInterleaved.To24bitBytesLE(), "", 3, [6], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("24,SI,BE,PCMS", newLineBeforeBytes: true, input24bitStereoInterleaved.To24bitBytesBE(), "", 3, [6], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("24,SI,LE,PCMU", newLineBeforeBytes: true, input24bitStereoInterleaved.Make24bitUnsigned().To24bitBytesLE(), "", 3, [6], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("24,SI,BE,PCMU", newLineBeforeBytes: true, input24bitStereoInterleaved.Make24bitUnsigned().To24bitBytesBE(), "", 3, [6], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		int[] input32bitLeft = Make32bit(expected16bitLeft);
		int[] input32bitRight = Make32bit(expected16bitRight);

		int[] input32bitStereoSplit = input32bitLeft.Concat(input32bitRight).ToArray();
		int[] input32bitStereoInterleaved = input32bitLeft.Zip(input32bitRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		OutputTestCase<HexByteEmitter, byte>("32,M,LE,PCMS", newLineBeforeBytes: true, input32bitLeft.ToBytesLE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("32,M,BE,PCMS", newLineBeforeBytes: true, input32bitLeft.ToBytesBE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("32,M,LE,PCMU", newLineBeforeBytes: true, input32bitLeft.MakeUnsigned().ToBytesLE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("32,M,BE,PCMU", newLineBeforeBytes: true, input32bitLeft.MakeUnsigned().ToBytesBE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("32,SI,LE,PCMS", newLineBeforeBytes: true, input32bitStereoInterleaved.ToBytesLE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("32,SI,BE,PCMS", newLineBeforeBytes: true, input32bitStereoInterleaved.ToBytesBE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("32,SI,LE,PCMU", newLineBeforeBytes: true, input32bitStereoInterleaved.MakeUnsigned().ToBytesLE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("32,SI,BE,PCMU", newLineBeforeBytes: true, input32bitStereoInterleaved.MakeUnsigned().ToBytesBE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		s_casesWriter.WriteLine("\t/* IEEE floating-point */");
		s_casesWriter.WriteLine();

		int[] inputSingleLeft = MakeSingle(expected16bitLeft);
		int[] inputSingleRight = MakeSingle(expected16bitRight);

		int[] inputSingleStereoSplit = inputSingleLeft.Concat(inputSingleRight).ToArray();
		int[] inputSingleStereoInterleaved = inputSingleLeft.Zip(inputSingleRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		OutputTestCase<HexByteEmitter, byte>("32,M,LE,IEEE", newLineBeforeBytes: true, inputSingleLeft.ToBytesLE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("32,M,BE,IEEE", newLineBeforeBytes: true, inputSingleLeft.ToBytesBE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("32,SS,LE,IEEE", newLineBeforeBytes: true, inputSingleStereoSplit.ToBytesLE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("32,SS,BE,IEEE", newLineBeforeBytes: true, inputSingleStereoSplit.ToBytesBE(), "", 4, [16, 16, 16, 16, 4], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("32,SI,LE,IEEE", newLineBeforeBytes: true, inputSingleStereoInterleaved.ToBytesLE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("32,SI,BE,IEEE", newLineBeforeBytes: true, inputSingleStereoInterleaved.ToBytesBE(), "", 4, [8], "EXPECT_16BIT | EXPECT_STEREO");

		double[] inputDoubleLeft = MakeDouble(expected16bitLeft);
		double[] inputDoubleRight = MakeDouble(expected16bitRight);

		double[] inputDoubleStereoSplit = inputDoubleLeft.Concat(inputDoubleRight).ToArray();
		double[] inputDoubleStereoInterleaved = inputDoubleLeft.Zip(inputDoubleRight, (a, b) => new[] { a, b }).SelectMany(x => x).ToArray();

		OutputTestCase<HexByteEmitter, byte>("64,M,LE,IEEE", newLineBeforeBytes: true, inputDoubleLeft.ToBytesLE(), "", 8, [32, 32, 32, 32, 8], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("64,M,BE,IEEE", newLineBeforeBytes: true, inputDoubleLeft.ToBytesBE(), "", 8, [32, 32, 32, 32, 8], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("64,SS,LE,IEEE", newLineBeforeBytes: true, inputDoubleStereoSplit.ToBytesLE(), "", 8, [32, 32, 32, 32, 8], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("64,SS,BE,IEEE", newLineBeforeBytes: true, inputDoubleStereoSplit.ToBytesBE(), "", 8, [32, 32, 32, 32, 8], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		OutputTestCase<HexByteEmitter, byte>("64,SI,LE,IEEE", newLineBeforeBytes: true, inputDoubleStereoInterleaved.ToBytesLE(), "", 8, [16], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("64,SI,BE,IEEE", newLineBeforeBytes: true, inputDoubleStereoInterleaved.ToBytesBE(), "", 8, [16], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		s_casesWriter.WriteLine("\t/* IT-compressed samples */");
		s_casesWriter.WriteLine();

		var it214Mono8bit = ITSampleCompressor.GetCompressedBytes(expected8bitLeft, doubleDelta: false);
		var it215Mono8bit = ITSampleCompressor.GetCompressedBytes(expected8bitLeft, doubleDelta: true);

		OutputTestCase<HexByteEmitter, byte>("8,M,LE,IT214", newLineBeforeBytes: true, it214Mono8bit, "", 1, [20], "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("8,M,LE,IT215", newLineBeforeBytes: true, it215Mono8bit, "", 1, [20], "EXPECT_8BIT | EXPECT_MONO");

		var it214Stereo8bit = ITSampleCompressor.GetCompressedBytes(expected8bitLeft, expected8bitRight, doubleDelta: false);
		var it215Stereo8bit = ITSampleCompressor.GetCompressedBytes(expected8bitLeft, expected8bitRight, doubleDelta: true);

		OutputTestCase<HexByteEmitter, byte>("8,SS,LE,IT214", newLineBeforeBytes: true, it214Stereo8bit, "", 1, [20], "EXPECT_8BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("8,SS,LE,IT215", newLineBeforeBytes: true, it215Stereo8bit, "", 1, [20], "EXPECT_8BIT | EXPECT_STEREO");

		var it214Mono16bit = ITSampleCompressor.GetCompressedBytes(expected16bitLeft, doubleDelta: false);
		var it215Mono16bit = ITSampleCompressor.GetCompressedBytes(expected16bitLeft, doubleDelta: true);

		OutputTestCase<HexByteEmitter, byte>("16,M,LE,IT214", newLineBeforeBytes: true, it214Mono16bit, "", 1, [20], "EXPECT_16BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,LE,IT215", newLineBeforeBytes: true, it215Mono16bit, "", 1, [20], "EXPECT_16BIT | EXPECT_MONO");

		var it214Stereo16bit = ITSampleCompressor.GetCompressedBytes(expected16bitLeft, expected16bitRight, doubleDelta: false);
		var it215Stereo16bit = ITSampleCompressor.GetCompressedBytes(expected16bitLeft, expected16bitRight, doubleDelta: true);

		OutputTestCase<HexByteEmitter, byte>("16,SS,LE,IT214", newLineBeforeBytes: true, it214Stereo16bit, "", 1, [20], "EXPECT_16BIT | EXPECT_STEREO");
		OutputTestCase<HexByteEmitter, byte>("16,SS,LE,IT215", newLineBeforeBytes: true, it215Stereo16bit, "", 1, [20], "EXPECT_16BIT | EXPECT_STEREO");

		OutputSeparator();

		// Heck is this?
		byte[] ptmDeltaBytes = new byte[expected16bitLeft.Length * 2];

		MemoryMarshal.Cast<short, byte>(expected16bitLeft.AsSpan()).CopyTo(ptmDeltaBytes);

		for (int i = ptmDeltaBytes.Length - 1; i > 1; i--)
			ptmDeltaBytes[i] -= ptmDeltaBytes[i - 1];

		OutputTestCase<HexByteEmitter, byte>("16,M,LE,PTM", newLineBeforeBytes: true, ptmDeltaBytes, "", 1, [20], "EXPECT_16BIT | EXPECT_MONO");

		OutputSeparator();

		// Digitrakker??
		byte[] mdlCompressedBytes8bit = MDLSampleCompressor.Compress(expected8bitLeft);
		byte[] mdlCompressedBytes16bit = MDLSampleCompressor.Compress(expected16bitLeft);

		OutputTestCase<HexByteEmitter, byte>("8,M,LE,MDL", newLineBeforeBytes: true, mdlCompressedBytes8bit, "", 1, [20], "EXPECT_8BIT | EXPECT_MONO");
		OutputTestCase<HexByteEmitter, byte>("16,M,LE,MDL", newLineBeforeBytes: true, mdlCompressedBytes16bit, "", 1, [20], "EXPECT_16BIT | EXPECT_MONO");
	}

	static void OutputSeparator()
	{
		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("/* ---------------------------------------------------- */");
		s_casesWriter.WriteLine();
	}

	interface IEmit<T> { void Emit(T value, string format); }

	class SByteEmitter() : IEmit<sbyte> { public void Emit(sbyte value, string format) { s_casesWriter.Write("{0" + format + "}", value); } }
	class ByteEmitter() : IEmit<byte> { public void Emit(byte value, string format) { s_casesWriter.Write("{0" + format + "}", value); } }
	
	class HexByteEmitter() : IEmit<byte> { public void Emit(byte value, string format) { s_casesWriter.Write("0x{0:X2}", value); } }

	static void OutputTestCase<T, U>(string formatBits, bool newLineBeforeBytes, U[] bytes, string format, int clusterSize, int[] newLineIntervals, string expectationFlags)
		where T : IEmit<U>, new()
	{
		var emitter = new T();

		string caseName = formatBits.Replace(',', '_');

		string caseDataName = "testcase_" + caseName;
		string caseFunctionName = "readsample_" + caseName;

		s_casesWriter.Write("static testcase_t {0} =", caseDataName);
		s_casesWriter.Write("\t{{ SF({0}),", formatBits);

		if (newLineBeforeBytes)
		{
			s_casesWriter.WriteLine();
			s_casesWriter.Write("\t\t{ ");
		}
		else
			s_casesWriter.Write(" { ");

		int intervalIndex = 0;
		int intervalStart = -1;

		for (int i = 0; i < bytes.Length; i++)
		{
			emitter.Emit(bytes[i], format);

			if (i + 1 >= bytes.Length)
				break;

			if ((intervalIndex < newLineIntervals.Length)
			 && (i == newLineIntervals[intervalIndex] + intervalStart))
			{
				s_casesWriter.WriteLine();
				s_casesWriter.Write("\t\t, ");

				intervalIndex = (intervalIndex + 1) % newLineIntervals.Length;
				intervalStart = i;
			}
			else if (((i + 1) % clusterSize) == 0)
				s_casesWriter.Write(", ");
			else
				s_casesWriter.Write(',');
		}

		s_casesWriter.WriteLine(" }}, {0}, {1} }};", bytes.Length, expectationFlags);
		s_casesWriter.WriteLine();
		s_casesWriter.WriteLine("testresult_t {0}()", caseFunctionName);
		s_casesWriter.WriteLine("{");
		s_casesWriter.WriteLine("\treturn test_readsample_case(&{0});", caseDataName);
		s_casesWriter.WriteLine("}");

		s_indexWriter.WriteLine("TEST_FUNC({0})", caseFunctionName);
	}

	static int[] Make24bit(short[] data)
	{
		int[] result = new int[data.Length];

		for (int i = 0; i < data.Length; i++)
			result[i] = data[i] << 8;

		// Test
		int max = 0xFF;

		for (int j = 0; j < result.Length; j++)
		{
			int l = result[j] / 256;

			if (l > max) max = l;
			if (-l > max) max = -l;
		}

		max = max * 2 + 1;

		for (int k = 0; k < result.Length; k++)
		{
			int d = (short)(result[k] * 256 / max);

			if (d != data[k])
				Debugger.Break();
		}

		return result;
	}

	static int[] Make32bit(short[] data)
	{
		int[] result = new int[data.Length];

		for (int i = 0; i < data.Length; i++)
			result[i] = data[i] << 16;

		// Test
		int max = 0xFFFF;

		for (int j = 0; j < result.Length; j++)
		{
			int l = result[j];

			if (l > max) max = l;
			if (-l > max) max = -l;
		}

		max = max / 32768 + 1;
		for (int k = 0; k < result.Length; k++)
		{
			int d = (short)(result[k] / max);

			if (d != data[k])
				Debugger.Break();
		}

		return result;
	}

	static int[] MakeSingle(short[] data)
	{
		int[] result = new int[data.Length];

		Span<float> resultFloats = MemoryMarshal.Cast<int, float>(result.AsSpan());

		for (int i = 0; i < data.Length; i++)
			resultFloats[i] = data[i] / 32768.0f;

		// Test
		for (int k = 0; k < data.Length; k++)
		{
			int d = (short)(resultFloats[k] * 32768.0f);

			if (d != data[k])
				Debugger.Break();
		}

		return result;
	}

	static double[] MakeDouble(short[] data)
	{
		double[] result = new double[data.Length];

		for (int i = 0; i < data.Length; i++)
			result[i] = data[i] / 32768.0;

		// Test
		for (int k = 0; k < data.Length; k++)
		{
			int d = (short)(result[k] * 32768.0);

			if (d != data[k])
				Debugger.Break();
		}

		return result;
	}
}

static class ArrayExtensions
{
	public static byte[] MakeUnsigned(this sbyte[] data)
	{
		byte[] ret = new byte[data.Length];

		for (int i = 0; i < ret.Length; i++)
			ret[i] = unchecked((byte)(data[i] ^ 0x80));

		return ret;
	}

	public static ushort[] MakeUnsigned(this short[] data)
	{
		ushort[] ret = new ushort[data.Length];

		for (int i = 0; i < ret.Length; i++)
			ret[i] = unchecked((ushort)(data[i] ^ 0x8000));

		return ret;
	}

	public static int[] Make24bitUnsigned(this int[] data)
	{
		int[] ret = new int[data.Length];

		for (int i = 0; i < ret.Length; i++)
			ret[i] = (data[i] & 0x00FFFFFF) ^ 0x800000;

		return ret;
	}

	public static int[] MakeUnsigned(this int[] data)
	{
		int[] ret = new int[data.Length];

		for (int i = 0; i < ret.Length; i++)
			ret[i] = data[i] ^ unchecked((int)0x80000000);

		return ret;
	}

	public static sbyte[] MakeDeltaMono(this sbyte[] data)
	{
		sbyte[] ret = new sbyte[data.Length];

		data.CopyTo(ret);

		for (int i = ret.Length - 1; i > 0; i--)
			ret[i] -= ret[i - 1];

		return ret;
	}

	public static short[] MakeDeltaMono(this short[] data)
	{
		short[] ret = new short[data.Length];

		data.CopyTo(ret);

		for (int i = ret.Length - 1; i > 0; i--)
			ret[i] -= ret[i - 1];

		return ret;
	}

	public static sbyte[] MakeDeltaStereo(this sbyte[] data)
	{
		sbyte[] ret = new sbyte[data.Length];

		data.CopyTo(ret);

		for (int i = ret.Length - 1; i > 1; i--)
			ret[i] -= ret[i - 2];

		return ret;
	}

	public static short[] MakeDeltaStereo(this short[] data)
	{
		short[] ret = new short[data.Length];

		data.CopyTo(ret);

		for (int i = ret.Length - 1; i > 1; i--)
			ret[i] -= ret[i - 2];

		return ret;
	}

	public static byte[] ToBytesLE(this short[] data)
	{
		byte[] ret = new byte[data.Length * 2];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt16LittleEndian(retSpan, data[i]);
			retSpan = retSpan.Slice(2);
		}

		return ret;
	}

	public static byte[] ToBytesBE(this short[] data)
	{
		byte[] ret = new byte[data.Length * 2];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt16BigEndian(retSpan, data[i]);
			retSpan = retSpan.Slice(2);
		}

		return ret;
	}

	public static byte[] ToBytesLE(this ushort[] data)
	{
		byte[] ret = new byte[data.Length * 2];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteUInt16LittleEndian(retSpan, data[i]);
			retSpan = retSpan.Slice(2);
		}

		return ret;
	}

	public static byte[] ToBytesBE(this ushort[] data)
	{
		byte[] ret = new byte[data.Length * 2];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteUInt16BigEndian(retSpan, data[i]);
			retSpan = retSpan.Slice(2);
		}

		return ret;
	}

	public static byte[] To24bitBytesLE(this int[] data)
	{
		byte[] ret = new byte[data.Length * 3];

		Span<byte> buf = stackalloc byte[4];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt32LittleEndian(buf, data[i]);

			buf.Slice(0, 3).CopyTo(retSpan);

			retSpan = retSpan.Slice(3);
		}

		return ret;
	}

	public static byte[] To24bitBytesBE(this int[] data)
	{
		byte[] ret = new byte[data.Length * 3];

		Span<byte> buf = stackalloc byte[4];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt32BigEndian(buf, data[i]);

			buf.Slice(1, 3).CopyTo(retSpan);

			retSpan = retSpan.Slice(3);
		}

		return ret;
	}

	public static byte[] ToBytesLE(this int[] data)
	{
		byte[] ret = new byte[data.Length * 4];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt32LittleEndian(retSpan, data[i]);

			retSpan = retSpan.Slice(4);
		}

		return ret;
	}

	public static byte[] ToBytesBE(this int[] data)
	{
		byte[] ret = new byte[data.Length * 4];

		Span<byte> buf = stackalloc byte[4];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteInt32BigEndian(retSpan, data[i]);

			retSpan = retSpan.Slice(4);
		}

		return ret;
	}

	public static byte[] ToBytesLE(this ReadOnlySpan<double> data)
	{
		byte[] ret = new byte[data.Length * 8];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteDoubleLittleEndian(retSpan, data[i]);

			retSpan = retSpan.Slice(8);
		}

		return ret;
	}

	public static byte[] ToBytesBE(this ReadOnlySpan<double> data)
	{
		byte[] ret = new byte[data.Length * 8];

		var retSpan = ret.AsSpan();

		for (int i = 0; i < data.Length; i++)
		{
			BinaryPrimitives.WriteDoubleBigEndian(retSpan, data[i]);

			retSpan = retSpan.Slice(8);
		}

		return ret;
	}
}
