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
    class Subscribe
    {
        public bool Combat { get; set; }
        public bool Extras { get; set; }
        public bool Squad { get; set; }
    }

    class ConnectionInfo
    {
        public bool CombatEnabled { get; set; }
        public bool ExtrasEnabled { get; set; }
        public bool ExtrasFound { get; set; }
        public bool SquadEnabled { get; set; }
    }

    class cbtevent
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

    class ag
    {
        public string name { get; set; }
        public UInt64 id { get; set; }
        public UInt32 prof { get; set; }
        public UInt32 elite { get; set; }
        public UInt32 self { get; set; }
        public UInt16 team { get; set; }
    }

    class PlayerInfo
    {
        public string accountName { get; set; }
        public string characterName { get; set; }
        public UInt32 profession { get; set; }
        public UInt32 elite { get; set; }
        public Byte role { get; set; }
        public Byte subgroup { get; set; }
        public bool inInstance { get; set; }
        public bool self { get; set; }
    }

    class ArcEvent
    {
        UInt64 id { get; set; }
        UInt64 revision { get; set; }
        public cbtevent ev { get; set; }
        public ag src { get; set; }
        public ag dst { get; set; }
        public string skillname { get; set; }
    }

    class SquadStatus
    {
        public string self { get; set; }
        public PlayerInfo[] members { get; set; }
    }

    internal class Handler
    {
        private class BridgeInfo
        {
            public string version { get; set; }
            public string extrasVersion { get; set; }
            public string arcVersion { get; set; }
            public bool arcLoaded { get; set; }
            public bool extrasFound { get; set; }
            public bool extrasLoaded { get; set; }
        }

        private class SquadPlayerEvent
        {
            public string source { get; set; }
            public PlayerInfo member { get; set; }
        }

        private class SquadData
        {
            public string trigger { get; set; }
            public SquadStatus status { get; set; }
            public SquadPlayerEvent add { get; set; }
            public SquadPlayerEvent remove { get; set; }
            public SquadPlayerEvent update { get; set; }
        }

        private class StatusEvent
        {
            public bool success { get; set; }
            public string error { get; set; }
        }

        private class BridgeEvent
        {
            public string type { get; set; }
            public StatusEvent status { get; set; }
            public BridgeInfo info { get; set; }
            public ArcEvent combat { get; set; }
            public PlayerInfo extras { get; set; }
            public SquadData squad { get; set; }
        }

        private enum MessageType : Byte
        {
            NONE = 0,
            Combat = 1,
            Extras = 2,
            Squad = 4
        }

        private static class EventType
        {
            public const string Info = "info";          // Bridge Information.
            public const string Status = "status";      // Status.
            public const string Squad = "squad";        // Squad information (initial squad data | player added, updated, or removed).
            public const string Combat = "combat";      // ArcDPS event.
            public const string Extras = "extras";      // Unofficial Extras event.
            public const string Closing = "closing";    // Server is closing the connection.
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

        public delegate void SquadInfo(SquadStatus squad);
        public delegate void PlayerChange(PlayerInfo player);
        public delegate void ArcMessage(ArcEvent evt);

        // Squad information events.
        public event SquadInfo OnSquadStatusEvent;
        public event PlayerChange OnPlayerAddedEvent;
        public event PlayerChange OnPlayerUpdateEvent;
        public event PlayerChange OnPlayerRemovedEvent;

        // Connection events.
        public delegate void ConntectedHandler(bool connected);
        public delegate void ConntectionInfoHandler(ConnectionInfo info);
        public event ConntectedHandler OnConnectionUpdate;
        public event ConntectionInfoHandler OnConnectionInfo;

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
            if (subscribe.Extras)
                _tData.EnabledTypes |= (Byte)MessageType.Extras;
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

        private static void PipeThreadMain(Object parameterData)
        {
            ThreadData tData = (ThreadData)parameterData;
            uint retries = 3;
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
                BridgeEvent bEvent = JsonSerializer.Deserialize<BridgeEvent>(infoData)!;
                BridgeInfo bInfo = bEvent.info;

                // Connection info received here, invoke callback.
                ConnectionInfo info = new ConnectionInfo()
                {
                    CombatEnabled = bInfo.arcLoaded,
                    ExtrasEnabled = bInfo.extrasLoaded,
                    ExtrasFound = bInfo.extrasFound,
                    SquadEnabled = bInfo.arcLoaded && bInfo.extrasLoaded
                };
                tData.Handle.OnConnectionInfo?.Invoke(info);

                // Both ArcDPS and Unofficial Extras are required.

                if (!bInfo.arcLoaded)
                {
                    // arc isn't available, exit.
                    // No need to retry here.

                    tData.Run = false;
                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    tData.Connected = false;
                    continue;
                }
                
                if (!bInfo.extrasLoaded)
                {
                    // Both ArcDPS and Unofficial Extras is needed for squad events.

                    if (retries > 0)
                    {
                        --retries;
                        Thread.Sleep(2000);
                    }
                    else
                        tData.Run = false;

                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    tData.Connected = false;
                    continue;
                }

                // Send subscribe data to server.
                Byte subscribe = tData.EnabledTypes; // id for squad messages.
                String sub = "{\"subscribe\":" + subscribe.ToString() + "}";
                WriteToPipe(tData.ClientStream, sub);

                // Read return status.
                String statusData = ReadFromPipe(tData.ClientStream);
                bEvent = JsonSerializer.Deserialize<BridgeEvent>(statusData)!;
                StatusEvent status = bEvent.status;
                if (!status.success)
                {
                    // Server closes pipe here.
                    // Error message is in status.error
                    tData.ClientStream.Close();
                    tData.ClientStream = null;
                    tData.Connected = false;
                    continue;
                }

                // Bridge is connected here, invoke callback.
                tData.Handle.OnConnectionUpdate?.Invoke(true);

                while (tData.Run && tData.ClientStream.IsConnected)
                {
                    String data = ReadFromPipe(tData.ClientStream);
                    if (data != "")
                    {
                        bEvent = JsonSerializer.Deserialize<BridgeEvent>(data)!;

                        if (bEvent.type == EventType.Closing)
                        {
                            // Server closing the connection, no more events will be sent.
                            tData.Run = false;
                            break;
                        }
                        else
                        {
                            tData.Handle.HandleBrideEvent(bEvent, tData);
                        }
                    }
                }

                // Stream is not connected here, or tData.Run is false.
                tData.ClientStream.Close();
                tData.ClientStream = null;
                tData.Connected = false;
                // Bridge disconnected here, invoke callback.
                tData.Handle.OnConnectionUpdate?.Invoke(false);
            }

            // Thread is ending, close stream if open.
            if (tData.ClientStream != null)
            {
                tData.ClientStream.Close();
                tData.ClientStream = null;
                tData.Connected = false;
                // Bridge disconnected here, invoke callback.
                tData.Handle.OnConnectionUpdate?.Invoke(false);
            }
        }

        private void HandleBrideEvent(BridgeEvent evt, ThreadData tData)
        {
            // "info" and "status" will never be handled here.

            if (evt.type == EventType.Squad && evt.squad != null)
            {
                HandleSquadEvent(evt.squad, tData);
            }
            else if (evt.type == EventType.Combat && evt.combat != null)
            {
                tData.Handle.OnArcEvent?.Invoke(evt.combat);
            }
            else if (evt.type == EventType.Extras && evt.extras != null)
            {
                tData.Handle.OnExtrasEvent?.Invoke(evt.extras);
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
            if (sq.trigger == SquadTriggers.Status && sq.status != null)
            {
                tData.Handle.OnSquadStatusEvent?.Invoke(sq.status);
            }
            else
            {
                // Player information events.

                if (sq.trigger == SquadTriggers.Update && sq.update != null)
                {
                    tData.Handle.OnPlayerUpdateEvent?.Invoke(sq.update.member);
                }
                else if (sq.trigger == SquadTriggers.Added && sq.add != null)
                {
                    tData.Handle.OnPlayerAddedEvent?.Invoke(sq.add.member);
                }
                else if (sq.trigger == SquadTriggers.Removed && sq.remove != null)
                {
                    tData.Handle.OnPlayerRemovedEvent?.Invoke(sq.remove.member);
                }
            }
        }
    }
}
