using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Ports;

namespace BinExchange
{

    public class BinExchange_Exception : Exception
    {
        public BinExchange_Exception()
        {
        }

        public BinExchange_Exception(string message)
            : base(message)
        {
        }

        public BinExchange_Exception(string message, Exception inner)
            : base(message, inner)
        {
        }
    }

    public class BinExchange_TimeoutException : Exception
    {
        public BinExchange_TimeoutException()
        {
        }

        public BinExchange_TimeoutException(string message)
            : base(message)
        {
        }

        public BinExchange_TimeoutException(string message, Exception inner)
            : base(message, inner)
        {
        }
    }


    class BinExchange
    {
        public int PackMaxLenght { get; private set; }
        public delegate void DataReceivedDelegate();

        public event DataReceivedDelegate DataReceived;



        SerialPort _serialPort;
        Queue<BinExchangePackage> PackFIFO;
        BinExchangeProtocol protocol;

        public BinExchange(int PackMaxLenght)
        {
            this.PackMaxLenght = PackMaxLenght;

            _serialPort = new SerialPort();
            PackFIFO = new Queue<BinExchangePackage>();
            protocol = new BinExchangeProtocol(PackMaxLenght);
        }

        public BinExchange() : this(32)
        {

        }

        /// <summary>
        /// Открыть порт
        /// </summary>
        /// <param name="Port">Имя COM-порта (например, "COM1")</param>
        /// <param name="Baud">Скорость передачи данных</param>
        public void Open(string Port, int Baud)
        {
            if (_serialPort.IsOpen)
                _serialPort.Close();

            _serialPort.PortName = Port;
            _serialPort.ReadTimeout = 500;
            _serialPort.WriteTimeout = 500;
            _serialPort.DataReceived += _serialPort_DataReceived;
            _serialPort.BaudRate = Baud;
            _serialPort.Open();
        }

        /// <summary>
        /// Прочитать пакет данных
        /// </summary>
        /// <returns>Полученные данные</returns>
        public byte[] Read(int Timeout)
        {
            int cntr = 0;

            while(PackFIFO.Count == 0)
            {
                System.Threading.Thread.Sleep(1);
                if(Timeout > 0)
                {
                    if(cntr >= Timeout)
                    {
                        throw new BinExchange_TimeoutException("Timeout error");
                    }

                    cntr++;
                }
            }


            BinExchangePackage ret = PackFIFO.Dequeue();
            if (ret.Status == BinExchangePackage.StatusCodes.OK)
            {
                return ret.Data;
            }
            else
            {
                throw new BinExchange_Exception("CRC error");
            }
        }

        public byte[] Read()
        {
            return Read(-1);
        }

        /// <summary>
        /// Отправить пакет данных
        /// </summary>
        /// <param name="data">Передаваемые данные. Длина не должна превышать .....</param>
        public void Write(byte[] data)
        {
            if (data.Length > PackMaxLenght)
                return;

            byte[] wr = protocol.Convert(data);

            _serialPort.Write(wr, 0, wr.Length);
        }


        /// <summary>
        /// Закрывает соединение порта.
        /// </summary>
        public void Close()
        {
            _serialPort.Close();
        }

        /// <summary>
        /// Возвращает массив имен последовательных портов для текущего компьютера.
        /// </summary>
        /// <returns>Массив имен последовательных портов для текущего компьютера.</returns>
        public string[] GetPortNames()
        {
            return SerialPort.GetPortNames();
        }

        private void _serialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            byte[] data = new byte[_serialPort.BytesToRead];

            _serialPort.Read(data, 0, data.Length);

            for(int i=0; i<data.Length; i++)
            {
                BinExchangePackage d = protocol.Parse(data[i]);
                if(d != null)
                {
                    PackFIFO.Enqueue(d);

                    DataReceived?.Invoke();
                }
            }
        }




    }

    class BinExchangePackage
    {
        public enum StatusCodes
        {
            OK = 0,
            CRC_ERROR = 1,
            PACK_TOO_LONG = 2
        };

        public UInt16 Len = 0;
        public UInt16 crc16 = 0;
        public StatusCodes Status;
        public byte[] Data;
    }

    class BinExchangeChar
    {
        public bool Start;
        public bool CharDetect;
        public byte Ch;


        public BinExchangeChar(bool start, bool ch_detect, byte ch)
        {
            Start = start;
            CharDetect = ch_detect;
            Ch = ch;
        }
    }


    class BinExchangeProtocol
    {
        const byte MAGIC_SYMBOL = 0xF4;

        BinExchangePackage pack;

        bool PrevMagicChar = false;

        int DroppedPacks;
        int DataCounter;
        int max_len;


        enum States
        {
            MAIN = 0,
            GET_HEADER,
            GET_DATA,
            GET_CRC
        };

        States state;

        public BinExchangeProtocol(int PackMaxLen)
        {
            state = States.MAIN;
            DroppedPacks = 0;

            max_len = PackMaxLen;
        }


        public byte[] Convert(byte[] data)
        {
            List<byte> ret = new List<byte>();

            UInt16 len = (UInt16)data.Length;
            UInt16 crc16;


            //Генерация заголовка
            ret.Add(MAGIC_SYMBOL);
            ret.Add(0);
            ret.Add((byte)(len));
            ret.Add((byte)(len >> 8));

            //Расчет CRC
            crc16 = Crc16(data, data.Length);

            //Добавляем данные
            for (int i = 0; i < data.Length; i++)
            {
                if (data[i] != MAGIC_SYMBOL)
                {
                    ret.Add(data[i]);
                }
                else
                {
                    ret.Add(MAGIC_SYMBOL);
                    ret.Add(MAGIC_SYMBOL);
                }
            }

            //Добавляем CRC
            byte tmp;

            tmp = (byte)(crc16);
            if(tmp == MAGIC_SYMBOL)
            {
                ret.Add(MAGIC_SYMBOL);
                ret.Add(MAGIC_SYMBOL);
            }
            else
            {
                ret.Add(tmp);
            }



            tmp = (byte)(crc16 >> 8);
            if (tmp == MAGIC_SYMBOL)
            {
                ret.Add(MAGIC_SYMBOL);
                ret.Add(MAGIC_SYMBOL);
            }
            else
            {
                ret.Add(tmp);
            }

            return ret.ToArray<byte>();
        }

        public BinExchangePackage Parse(byte ch)
        {
            BinExchangeChar dta = GetByte(ch);

            switch (state)
            {
                case States.MAIN: //ищем начало пакета 
                    if (dta.Start) //если нашли начало пакета
                    {
                        pack = new BinExchangePackage();
                        DataCounter = 0;
                        state = States.GET_HEADER;
                    }
                    break;
                ///////////////////////////
                case States.GET_HEADER: //Читаем заголовок
                    if(dta.CharDetect) //если прочли символ
                    {
                        if (DataCounter == 0) //если читаем 1-й байт
                        {
                            pack.Len = dta.Ch;
                            DataCounter++;
                        }
                        else
                        {
                            pack.Len |= (UInt16)(dta.Ch << 8);
                            DataCounter = 0;

                            if(pack.Len > max_len) //пакет слишком большой
                            {
                                DropsIncr();

                                pack.Status = BinExchangePackage.StatusCodes.PACK_TOO_LONG;
                                state = States.MAIN;
                                return pack;
                            }
                            else if (pack.Len > 0) //если пакет не пустой
                            {
                                pack.Data = new byte[pack.Len];
                                state = States.GET_DATA;
                            }
                            else
                            {
                                state = States.GET_CRC;
                            }
                        }
                    }
                    else if(dta.Start) //если обнаружили начало пакета, хотя тут должен быть заголовок
                    {
                        DropsIncr();

                        //начинаем сначала
                        pack = new BinExchangePackage();
                        DataCounter = 0;
                        state = States.GET_HEADER;
                    }
                    break;
                ///////////////////////////
                case States.GET_DATA: //Читаем данные
                    if (dta.CharDetect) //если прочли символ
                    {
                        pack.Data[DataCounter] = dta.Ch;
                        DataCounter++;
                        if(DataCounter >= pack.Len) //если прочли все
                        {
                            DataCounter = 0;
                            state = States.GET_CRC;
                        }
                    }
                    else if (dta.Start) //если обнаружили начало пакета, хотя тут должны быть данные
                    {
                        DropsIncr();

                        //начинаем сначала
                        pack = new BinExchangePackage();
                        DataCounter = 0;
                        state = States.GET_HEADER;
                    }
                    break;
                ///////////////////////////
                case States.GET_CRC: //принимаем CRC
                    if (dta.CharDetect) //если прочли символ
                    {
                        if (DataCounter == 0) //если читаем 1-й байт
                        {
                            pack.crc16 = dta.Ch;
                            DataCounter++;
                        }
                        else
                        {
                            pack.crc16 |= (UInt16)(dta.Ch << 8);
                            DataCounter = 0;

                            ///// ToDo: добавить ошибку CRC

                            //Проверяем CRC
                            if (Crc16(pack.Data, pack.Len) == pack.crc16)
                            {
                                state = States.MAIN;
                                pack.Status = BinExchangePackage.StatusCodes.OK;
                                return pack;
                            }
                            else
                            {
                                DropsIncr();

                                pack.Status = BinExchangePackage.StatusCodes.CRC_ERROR;
                                state = States.MAIN;
                                return pack;
                            }
                        }
                    }
                    else if (dta.Start) //если обнаружили начало пакета, хотя тут должен быть заголовок
                    {
                        DropsIncr();

                        //начинаем сначала
                        pack = new BinExchangePackage();
                        DataCounter = 0;
                        state = States.GET_HEADER;
                    }
                    break;
            }

            return null;
        }

        void DropsIncr()
        {
            if (DroppedPacks < int.MaxValue)
                DroppedPacks++;
        }

        BinExchangeChar GetByte(byte ch)
        {
            BinExchangeChar ret;

            if (PrevMagicChar == false)
            {
                if (ch != MAGIC_SYMBOL)
                {
                    ret = new BinExchangeChar(false, true, ch); //Не спец. симпол, просто данные
                    PrevMagicChar = false;
                }
                else
                {
                    ret = new BinExchangeChar(false, false, ch); //Спец. символ, ждем следующий
                    PrevMagicChar = true;
                }
            }
            else
            {
                if (ch != MAGIC_SYMBOL)
                {
                    ret = new BinExchangeChar(true, false, ch); //Начало пакета
                    PrevMagicChar = false;
                }
                else
                {
                    ret = new BinExchangeChar(false, true, ch); //Экранирование спец. символа
                    PrevMagicChar = false;
                }
            }
            return ret;
        }

        


        /**********************************************/

        /*
          Name  : CRC-16
          Poly  : 0x8005    x^16 + x^15 + x^2 + 1
          Init  : 0xFFFF
          Revert: true
          XorOut: 0x0000
          Check : 0x4B37 ("123456789")
          MaxLen: 4095 bytes
        */
        UInt16[] Crc16Table = new UInt16[256] {
            0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
            0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
            0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
            0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
            0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
            0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
            0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
            0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
            0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
            0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
            0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
            0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
            0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
            0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
            0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
            0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
            0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
            0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
            0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
            0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
            0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
            0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
            0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
            0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
            0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
            0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
            0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
            0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
            0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
            0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
            0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
            0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
        };


        /*
        uint16_t Crc16Reset(void)
        {
            return 0xFFFF;
        }*/

        UInt16 Crc16(byte[] pcBlock, int len)
        {
            UInt16 crc = 0xFFFF;

            for (int i = 0; i < len; i++)
                crc = (UInt16)((crc >> 8) ^ Crc16Table[((crc & 0xFF) ^ pcBlock[i])]);

            return crc;
        }


    }
}
