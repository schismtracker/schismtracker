namespace MakeReadSample;

class BitWriter(Stream underlying) : IDisposable
{
	public int BytesWritten { get; private set; }

	bool _isOpen = true;
	byte _currentByte;
	byte _nextBit = 1;

	public void WriteBit(bool value)
	{
		if (!_isOpen)
			throw new InvalidOperationException("BitWriter is closed");

		if (value)
			_currentByte |= _nextBit;

		_nextBit <<= 1;

		if (_nextBit == 0)
		{
			underlying.WriteByte(_currentByte);
			BytesWritten++;
			_currentByte = 0;
			_nextBit = 1;
		}
	}

	public void Write(int value, int width)
	{
		int bit = 1;

		for (int i = 0; i < width; i++)
		{
			WriteBit((value & bit) != 0);
			bit <<= 1;
		}
	}

	public void Close()
	{
		if (!_isOpen)
			throw new InvalidOperationException("BitWriter is closed");

		if (_nextBit > 1)
		{
			underlying.WriteByte(_currentByte);
			BytesWritten++;
		}

		_isOpen = false;
	}

	public void Dispose()
	{
		if (_isOpen)
			Close();
	}
}
