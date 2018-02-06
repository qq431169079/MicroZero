using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using Agebull.Common.Logging;
using Newtonsoft.Json;

namespace ZeroNet.Http.Route
{
    /// <summary>
    /// 路由计数器
    /// </summary>
    internal class RouteCounter
    {
        /// <summary>
        /// 开始时间
        /// </summary>
        public DateTime Start { get; set; }

        /// <summary>
        /// 开始计数
        /// </summary>
        /// <returns></returns>
        public static RouteCounter Begin()
        {
            return new RouteCounter
            {
                Start = DateTime.Now
            };
        }

        /// <summary>
        /// 开始计数
        /// </summary>
        /// <returns></returns>
        public void End(RouteData data)
        {
            try
            {
                var tm = (DateTime.Now - Start).TotalMilliseconds;
                if (tm > 200)
                    LogRecorder.Warning($"{data.HostName}/{data.ApiName}:执行时间异常({tm:F2}ms):");

                if (tm > AppConfig.Config.SystemConfig.WaringTime)
                    RouteRuntime.RuntimeWaring(data.HostName, data.ApiName, $"执行时间异常({tm:F0}ms)");

                long unit = DateTime.Today.Year * 1000000 + DateTime.Today.Month * 10000 + DateTime.Today.Day * 100 + DateTime.Now.Hour ;
                if (unit != Unit)
                {
                    Unit = unit;
                    Save();
                    Station = new CountItem();
                }

                Station.SetValue(tm, data);
                if (string.IsNullOrWhiteSpace(data.HostName))
                    return;
                CountItem host;
                lock (Station)
                {
                    if (!Station.Items.TryGetValue(data.HostName, value: out host))
                        Station.Items.Add(data.HostName, host = new CountItem());
                }
                host.SetValue(tm,data);

                if (string.IsNullOrWhiteSpace(data.ApiName))
                    return;
                CountItem api;
                lock (host)
                {
                    if (!host.Items.TryGetValue(data.ApiName, out api))
                        host.Items.Add(data.ApiName, api = new CountItem());
                }
                api.SetValue(tm, data);
            }
            catch (Exception e)
            {
                LogRecorder.Exception( e);
            }
        }


        /// <summary>
        /// 保存为性能日志
        /// </summary>
        public static void Save()
        {
            try
            {
                File.AppendAllText(Path.Combine(TxtRecorder.LogPath, $"{Unit}.count.log"), JsonConvert.SerializeObject(Station, Formatting.Indented));
            }
            catch 
            {
            }
        }
        /// <summary>
        /// 计数单元
        /// </summary>
        public static long Unit = DateTime.Today.Year * 1000000 + DateTime.Today.Month * 10000 + DateTime.Today.Day * 100 + DateTime.Now.Hour;

        /// <summary>
        /// 计数根
        /// </summary>
        public static CountItem Station { get; set; } = new CountItem();
    }

    /// <summary>
    /// 路由计数器节点
    /// </summary>
    [JsonObject(MemberSerialization.OptIn), DataContract]
    internal class CountItem
    {
        public void SetValue(double tm, RouteData data)
        {
            this.LastTime = tm;
            this.Count += 1;

            if (data.Status == RouteStatus.DenyAccess)
            {
                this.Deny += 1;
            }
            else if (data.Status >= RouteStatus.LocalError)
            {
                this.Error += 1;
            }
            else
            {
                if (tm > AppConfig.Config.SystemConfig.WaringTime)
                {
                    this.TimeOut += 1;
                }
                this.TotalTime += tm;
                this.AvgTime = this.TotalTime / this.Count;
                if (this.MaxTime < tm)
                    this.MaxTime = tm;
                if (this.MinTime > tm)
                    this.MinTime = tm;
            }
        }

        /// <summary>
        /// 最长时间
        /// </summary>
        [DataMember, JsonProperty]
        public double MaxTime { get; set; } = double.MinValue;

        /// <summary>
        /// 最短时间
        /// </summary>
        [DataMember, JsonProperty]
        public double MinTime { get; set; } = Double.MaxValue;
        /// <summary>
        /// 总时间
        /// </summary>
        [DataMember, JsonProperty]
        public double TotalTime { get; set; }
        /// <summary>
        /// 平均时间
        /// </summary>
        [DataMember, JsonProperty]
        public double AvgTime { get; set; }

        /// <summary>
        /// 最后时间
        /// </summary>
        [DataMember, JsonProperty]
        public double LastTime { get; set; }
        
        /// <summary>
        /// 总次数
        /// </summary>
        [DataMember, JsonProperty]
        public int Count { get; set; }

        /// <summary>
        /// 错误次数
        /// </summary>
        [DataMember, JsonProperty]
        public int TimeOut { get; set; }

        /// <summary>
        /// 错误次数
        /// </summary>
        [DataMember, JsonProperty]
        public int Error { get; set; }

        /// <summary>
        /// 拒绝次数
        /// </summary>
        [DataMember, JsonProperty]
        public int Deny { get; set; }
        
        /// <summary>
        /// 子级
        /// </summary>
        [DataMember, JsonProperty]
        public Dictionary<string, CountItem> Items { get; set; } = new Dictionary<string, CountItem>(StringComparer.OrdinalIgnoreCase);
    }
}