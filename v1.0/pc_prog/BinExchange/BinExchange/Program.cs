using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BinExchange
{
    class Program
    {
        static BinExchange BinEx = new BinExchange(32);

        static void Main(string[] args)
        {
            Random rnd = new Random();
            

            BinEx.Open("COM5", 9600);

            BinEx.DataReceived += Bin_DataReceived;

            byte[] tx = new byte[32];

            rnd.NextBytes(tx);
            BinEx.Write(tx);



            Console.ReadKey();
        }

        
        private static void Bin_DataReceived()
        {
            byte[] rx = BinEx.Read();

            for(int i=0; i<rx.Length;i++)
            {
                Console.Write(rx[i].ToString() + " ");
            }

            Console.WriteLine();
            
        }
        
    }
}
