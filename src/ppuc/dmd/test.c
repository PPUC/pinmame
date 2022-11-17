public bool StreamBytes(byte[] pBytes, int nBytes)
{
    if (_serialPort.IsOpen)
    {
        try
        {
            int signal = 0;
            int bytesToWrite = Math.Min(nBytes, MAX_SERIAL_WRITE_AT_ONCE - N_CTRL_CHARS);
            while (signal != 'R') {
                signal = _serialPort.ReadByte();
                if (signal == 'E') return false;
            }
            _serialPort.Write(CtrlCharacters, 0, N_CTRL_CHARS);
            _serialPort.Write(pBytes, 0, bytesToWrite);
            while (signal != 'A') {
                signal = _serialPort.ReadByte();
                if (signal == 'E') return false;
            }

            int remainingBytes = nBytes - bytesToWrite;
            while (remainingBytes > 0) {
                bytesToWrite = Math.Min(remainingBytes, MAX_SERIAL_WRITE_AT_ONCE);
                _serialPort.Write(pBytes, nBytes - remainingBytes, bytesToWrite);
                remainingBytes -= bytesToWrite;
                while (signal != 'A') {
                    signal = _serialPort.ReadByte();
                    if (signal == 'E') return false;
                }
            }
            return true;
        }
        catch
        {
            if (_serialPort.IsOpen)
            {
                _serialPort.DiscardOutBuffer();
            }
            return false;
        }
    }
    return false;
}



    // Send a big buffer in several transfers to avoid corrupted data
            byte[] pBytes2 = new byte[MAX_SERIAL_WRITE_AT_ONCE];
            byte[] pReadyByte = new byte[1];
            // 4 pour la synchro
             if (_serialPort.IsOpen)
            {
                int remainTrans = nBytes; // la totalité - les bytes de synchro
                try
                {
                    // premier transfert
                    for (int i = 0; i < N_CTRL_CHARS; i++) pBytes2[i] = CtrlCharacters[i];
                    int ntowritetotal = nBytes + N_CTRL_CHARS;
                    Buffer.BlockCopy(pBytes, 0, pBytes2, N_CTRL_CHARS, MAX_SERIAL_WRITE_AT_ONCE);
                    while (pReadyByte[0]!='R') _serialPort.Read(pReadyByte, 0, 1);
                    pReadyByte[0] = 0;
                    _serialPort.Write(pBytes2, 0, MAX_SERIAL_WRITE_AT_ONCE);
                    int tpos = MAX_SERIAL_WRITE_AT_ONCE - N_CTRL_CHARS;
                    while (tpos < ntowritetotal)
                    {
                        int ntowrite = Math.Min(ntowritetotal - tpos, MAX_SERIAL_WRITE_AT_ONCE);
                        while (pReadyByte[0]!='R') _serialPort.Read(pReadyByte, 0, 1);
                        pReadyByte[0] = 0;
                        _serialPort.Write(pBytes, tpos, ntowrite);
                        tpos += ntowrite;
                    }

                    return true;
                }
                catch
                {
                    if (_serialPort.IsOpen)
                    {
                        _serialPort.DiscardInBuffer();
                        _serialPort.DiscardOutBuffer();
                    }
                    return false;
                };
            }
            return false;
        }
