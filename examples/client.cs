//
//  examples/client.cs
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-22.
//

using System;
using System.Text;
using System.Threading;
using System.IO.Pipes;
using System.Security.Principal;
using System.Text.Json;

namespace BridgeHandler
{
    internal class Handler
    {
        public class cbtevent
        {
            public UInt64 time { get; set; }
            public UInt64 src_agent { get; set; }
            public UInt64 dst_agent { get; set; }
            public Int32 value { get; set; }
            public Int32 buff_dmg { get; set; }
            public UInt32 overstack_value { get; set; }
            public UInt32 skillid { get; set; }
            public UInt16 src_instid { get; set; }
            public UInt16 dst_instid { get; set; }
            public UInt16 src_master_instid { get; set; }
            public UInt16 dst_master_instid { get; set; }
            public Byte iff { get; set; }
            public Byte buff { get; set; }
            public Byte result { get; set; }
            public Byte is_activation { get; set; }
            public Byte is_buffremove { get; set; }
            public Byte is_ninety { get; set; }
            public Byte is_fifty { get; set; }
            public Byte is_moving { get; set; }
            public Byte is_statechange { get; set; }
            public Byte is_flanking { get; set; }
            public Byte is_shields { get; set; }
            public Byte is_offcycle { get; set; }
        }

        public class ag
        {
            public string name { get; set; }
            public UInt64 id { get; set; }
            public UInt32 prof { get; set; }
            public UInt32 elite { get; set; }
            public UInt32 self { get; set; }
            public UInt16 team { get; set; }
        }

        public class PlayerInfo
        {
            public string AccountName { get; set; }
            public string CharacterName { get; set; }
            public UInt32 Profession { get; set; }
            public UInt32 Elite { get; set; }
            public Byte Role { get; set; }
            public Byte Subgroup { get; set; }
        }

        public class ArcEvent
        {
            UInt64 id { get; set; }
            UInt64 revision { get; set; }
            public cbtevent ev { get; set; }
            public ag src { get; set; }
            public ag dst { get; set; }
            public string skillname { get; set; }
        }

        private class SquadData
        {
            public string type { get; set; }
            public string self { get; set; }
            public string trigger { get; set; }
            public PlayerInfo[] members { get; set; }
            public PlayerInfo member { get; set; }
        }

        private class BridgeEvent
        {
            public string type { get; set; }
            public ArcEvent combat { get; set; }
            public PlayerInfo extra { get; set; }
            public SquadData squad { get; set; }
        }

        public static string ReadFromPipe(NamedPipeClientStream stream)
        {
            StringBuilder messageBuilder = new StringBuilder();
            string messageChunk = string.Empty;
            byte[] messageBuffer = new byte[5];
            do
            {
                stream.Read(messageBuffer, 0, messageBuffer.Length);
                messageChunk = Encoding.UTF8.GetString(messageBuffer);
                messageBuilder.Append(messageChunk);
                messageBuffer = new byte[messageBuffer.Length];
            }
            while (!stream.IsMessageComplete);

            string data = messageBuilder.ToString();
            data = data.Replace("\0", "");
            return data;
        }

        public static void WriteToPipe(NamedPipeClientStream stream, string message)
        {
            byte[] messageBytes = Encoding.UTF8.GetBytes(message);
            stream.Write(messageBytes, 0, messageBytes.Length);
        }

        public class BridgeInfo
        {
            public string type { get; set; }
            public string Version { get; set; }
            public string ExtraVersion { get; set; }
            public string ArcVersion { get; set; }
            public bool ArcLoaded { get; set; }
            public bool ExtraLoaded { get; set; }
        }

        public class SquadInfoEvent
        {
            public string Self { get; set; }
            public PlayerInfo[] Members { get; set; }
        }

        public class Subscribe
        {
            public bool Combat { get; set; }
            public bool Extra { get; set; }
            public bool Squad { get; set; }
        }

        private enum MessageType : Byte
        {
            NONE = 0,
            Combat = 1,
            Extra = 2,
            Squad = 4
        }

        public delegate void SquadInfo(SquadInfoEvent squad);
        public delegate void PlayerChange(PlayerInfo player);
        public delegate void ArcMessage(ArcEvent evt);

        // Squad information events.
        public event SquadInfo OnSquadInfoEvent;
        public event PlayerChange OnPlayerAddedEvent;
        public event PlayerChange OnPlayerUpdateEvent;
        public event PlayerChange OnPlayerRemovedEvent;

        // ArcDPS event.
        public event ArcMessage OnArcEvent;

        // Unofficial Extras event.
        public event PlayerChange OnExtrasEvent;

        private class ThreadData
        {
            public NamedPipeClientStream ClientStream = null;
            public Handler Handle = null;
            public bool Run { get; set; }
            public bool Connected { get; set; }
            public Byte EnabledTypes { get; set; }
        }

        private Thread _t;
        private ThreadData _tData;

        public void Start(Subscribe subscribe)
        {
            _t = new Thread(PipeThreadMain);
            _tData = new ThreadData();
            _tData.Handle = this;

            _tData.EnabledTypes = (Byte)MessageType.NONE;
            if (subscribe.Combat)
                _tData.EnabledTypes |= (Byte)MessageType.Combat;
            if (subscribe.Extra)
                _tData.EnabledTypes |= (Byte)MessageType.Extra;
            if (subscribe.Squad)
                _tData.EnabledTypes |= (Byte)MessageType.Squad;
            
            _tData.Run = true;
            _tData.Connected = false;
            _t.Start(_tData);
        }

        public void Stop()
        {
            _tData.Run = false;

            if (_tData.ClientStream != null)
            {
                _tData.ClientStream.Close();
            }
                
            _t.Join();
        }

        public bool IsConnected()
        {
            return _tData.Connected;
        }

        public bool IsRunning()
        {
            return _tData.Run;
        }

        private class SubscribeStatus
        {
            public bool success { get; set; }
            public string error { get; set; }
        }

        private static void PipeThreadMain(Object parameterData)
        {
            ThreadData tData = (ThreadData)parameterData;
            while (tData.Run)
            {
                tData.ClientStream = new NamedPipeClientStream(".", "arcdps-bridge", PipeDirection.InOut, PipeOptions.None, TokenImpersonationLevel.Impersonation);
                if (tData.ClientStream == null)
                    continue;
                
                tData.ClientStream.Connect();
                tData.ClientStream.ReadMode = PipeTransmissionMode.Message;

                if (!tData.ClientStream.IsConnected)
                {
                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    continue;
                }

                // ClientStream is connected here.
                tData.Connected = true;

                // Read BridgeInfo.
                String infoData = ReadFromPipe(tData.ClientStream);
                BridgeInfo bInfo = JsonSerializer.Deserialize<BridgeInfo>(infoData)!;
                if (!bInfo.ArcLoaded && !bInfo.ExtraLoaded)
                {
                    // Both ArcDPS and Unofficial Extras is needed for squad events.
                    tData.Run = false;
                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    tData.Connected = false;
                    continue; // Thread will close after this.
                }

                // Send subscribe data to server.
                Byte subscribe = tData.EnabledTypes; // id for squad messages.
                String sub = "{\"subscribe\":" + subscribe.ToString() + "}";
                WriteToPipe(tData.ClientStream, sub);

                // Read return status.
                String statusStr = ReadFromPipe(tData.ClientStream);
                SubscribeStatus status = JsonSerializer.Deserialize<SubscribeStatus>(statusStr)!;
                if (!status.success)
                {
                    // Server closes pipe here.
                    // Error message is in status.error
                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    tData.Connected = false;
                    continue;
                }

                while (tData.Run && tData.ClientStream.IsConnected)
                {
                    String data = ReadFromPipe(tData.ClientStream);
                    BridgeEvent evt = JsonSerializer.Deserialize<BridgeEvent>(data)!;
                    tData.Handle.HandleBrideEvent(evt, tData);
                }

                // Stream is not connected here, or tData.Run is false.
                tData.ClientStream.Close();
                tData.ClientStream = null;
                tData.Connected = false;
            }

            // Thread is ending, close stream if open.
            if (tData.ClientStream != null)
            {
                tData.ClientStream.Close();
                tData.ClientStream = null;
                tData.Connected = false;
            }
        }
        private static class EventType
        {
            public const string Info = "Info";      // Bridge Information.
            public const string Squad = "Squad";    // Squad information (initial squad data | player added, updated, or removed).
            public const string Combat = "Combat";  // ArcDPS event.
            public const string Extra = "Extra";    // Unofficial Extras event.
        }

        private void HandleBrideEvent(BridgeEvent evt, ThreadData tData)
        {
            // "Info" will never be handled here.

            if (evt.type == EventType.Squad && evt.squad != null)
            {
                HandleSquadEvent(evt.squad, tData);
            }
            else if (evt.type == EventType.Combat && evt.combat != null)
            {
                tData.Handle.OnArcEvent?.Invoke(evt.combat);
            }
            else if (evt.type == EventType.Extra && evt.extra != null)
            {
                tData.Handle.OnExtrasEvent?.Invoke(evt.extra);
            }
        }

        private static class SquadTriggers
        {
            public const string Status = "status";
            public const string Update = "update";
            public const string Added = "add";
            public const string Removed = "remove";
        }

        private void HandleSquadEvent(SquadData sq, ThreadData tData)
        {
            if (sq.trigger == SquadTriggers.Status)
            {
                SquadInfoEvent squadInfoEvent = new SquadInfoEvent
                {
                    Self = sq.self,
                    Members = sq.members
                };
                tData.Handle.OnSquadInfoEvent?.Invoke(squadInfoEvent);
            }
            else
            {
                // Player information events.

                if (sq.trigger == SquadTriggers.Update)
                {
                    tData.Handle.OnPlayerUpdateEvent?.Invoke(sq.member);
                }
                else if (sq.trigger == SquadTriggers.Added)
                {
                    tData.Handle.OnPlayerAddedEvent?.Invoke(sq.member);
                }
                else if (sq.trigger == SquadTriggers.Removed)
                {
                    tData.Handle.OnPlayerRemovedEvent?.Invoke(sq.member);
                }
            }
        }
    }
}
