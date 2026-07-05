# lwIP src/core/ipv4/ip4.c 完整解析 + Mermaid流程图
## 一、文件整体概述
`ip4.c` 是 lwIP IPv4 协议栈**核心层实现文件**，承担三层核心职责：
1. **收包处理**：`ip4_input()` 网卡收包入口，IP头校验、分片重组、转发、上送TCP/UDP/ICMP/IGMP
2. **发包处理**：`ip4_output()`/`ip4_output_if()` 上层发包入口，路由查找、IP头封装、分片、网卡下发
3. **路由与转发**：`ip4_route()` 路由查询、`ip4_forward()` 跨网卡IP转发、组播默认网口管理
配套依赖：
- 分片重组：`ip4_frag.c`（`ip4_reass()` 分片重组、`ip4_frag()` 发包分片）
- 辅助：路由钩子、组播、DHCP特殊放行、IP校验和、统计计数

编译开关：`LWIP_IPV4=1` 才编译本文件。

# 二、三大核心函数 Mermaid 流程图
## 1. ip4_input() 收包总流程（网卡驱动回调入口）


```mermaid
flowchart TD
    %% =====================
    %% ip4_input() 竖向流程图
    %% 修复点：少汇合、短节点、竖向展开、统一DROP节点
    %% =====================

    A[网卡驱动收包]
    B[进入 ip4_input 函数]
    C[接收统计计数]

    C0{IP版本字段 == 4 ?}

    DROP[释放pbuf<br/>统计丢弃<br/>返回ERR_OK]
    END[流程结束]

    C0 -- 否 --> DROP
    DROP --> END

    C0 -- 是 --> C1{输入钩子拦截报文 ?}

    C1 -- 拦截 --> DROP
    C1 -- 放行 --> S2[读取IP头长度<br/>读取报文总长度]

    S2 --> S3[裁剪pbuf多余尾部数据]
    S3 --> C2{IP头是否合法 ?}

    C2 -- 非法 --> DROP
    C2 -- 合法 --> C3{硬件校验和是否完成 ?}

    C3 -- 未校验 --> CHK[软件校验IP头]
    CHK --> C4{校验是否通过 ?}

    C4 -- 失败 --> DROP
    C4 -- 通过 --> S4[缓存源IP与目的IP]
    C3 -- 已校验 --> S4

    S4 --> C5{目的IP是否为组播 ?}

    %% 组播路径
    C5 -- 是 --> C6{IGMP已加入对应组 ?}
    C6 -- 是 --> M1[netif = 入网卡]
    C6 -- 否 --> M2[netif = NULL]

    %% 单播路径
    C5 -- 否 --> C7{入网卡是否匹配本机IP ?}
    C7 -- 匹配 --> U1[netif = 入网卡]
    C7 -- 不匹配 --> N7[遍历所有网卡]
    N7 --> C8{是否找到匹配网卡 ?}
    C8 -- 是 --> U2[netif = 匹配网卡]
    C8 -- 否 --> U3[netif = NULL]

    %% 分阶段汇合，避免渲染失败
    M1 --> TMP1
    M2 --> TMP1
    U1 --> TMP1
    TMP1 --> TMP2
    U2 --> TMP2
    U3 --> TMP2

    TMP2 --> C9{netif 是否为空 ?}

    C9 -- 是且UDP端口68 --> DHCP_OK[强制 netif = 入网卡]
    C9 -- 其他情况 --> SRC_CHECK[校验源IP合法性]
    DHCP_OK --> SRC_CHECK

    SRC_CHECK --> C10{源IP是否非法 ?}
    C10 -- 非法 --> DROP
    C10 -- 合法 --> C11{是否无本机匹配网卡 ?}

    %% 转发路径
    C11 -- 无匹配 --> C12{是否允许IP转发 ?}
    C12 -- 允许 --> FW[调用 ip4_forward]
    C12 -- 禁止 --> DROP
    FW --> END

    %% 本机接收路径
    C11 -- 匹配本机 --> C13{是否为IP分片报文 ?}

    C13 -- 是分片 --> C14{是否开启分片重组 ?}
    C14 -- 开启 --> REASS[调用 ip4_reass]
    REASS --> C15{重组是否完成 ?}

    C15 -- 未完成 --> END
    C15 -- 完成 --> HDR_UPDATE[更新IP头指针]

    C14 -- 关闭重组 --> DROP
    C13 -- 完整报文 --> OPT_CHECK
    HDR_UPDATE --> OPT_CHECK

    OPT_CHECK{禁用IP选项<br/>且携带选项 ?}
    OPT_CHECK -- 违规 --> DROP
    OPT_CHECK -- 正常 --> FILL_CTX[填充全局上下文]

    FILL_CTX --> RAW_PROCESS[raw_input 原始套接字处理]
    RAW_PROCESS --> C17{RAW是否消费报文 ?}

    C17 -- 已消费 --> END
    C17 -- 未消费 --> DEL_HDR[剥离IP头部]

    DEL_HDR --> DISPATCH{根据协议号分发}

    DISPATCH -->|UDP| UDP_IN[udp_input]
    DISPATCH -->|TCP| TCP_IN[tcp_input]
    DISPATCH -->|ICMP| ICMP_IN[icmp_input]
    DISPATCH -->|IGMP| IGMP_IN[igmp_input]
    DISPATCH -->|未知协议| UNKNOWN[回复ICMP协议不可达]

    UDP_IN --> CLR
    TCP_IN --> CLR
    ICMP_IN --> CLR
    IGMP_IN --> CLR
    UNKNOWN --> CLR

    CLR[清空全局 ip_data 缓存]
    CLR --> END

    %% 样式设置：让节点更清楚
    classDef startend fill:#E8F7EF,stroke:#20A464,stroke-width:2px,color:#111;
    classDef process fill:#EAF3FF,stroke:#2F80ED,stroke-width:1.5px,color:#111;
    classDef decision fill:#FFF4D6,stroke:#F2A900,stroke-width:2px,color:#111;
    classDef drop fill:#FFECEC,stroke:#D92D20,stroke-width:2px,color:#111;
    classDef forward fill:#F6E8FF,stroke:#9D28AC,stroke-width:1.5px,color:#111;

    class A,B,C,S2,S3,S4,N7,DHCP_OK,SRC_CHECK,HDR_UPDATE,FILL_CTX,RAW_PROCESS,DEL_HDR,CLR,FW,REASS,M1,M2,U1,U2,U3,TMP1,TMP2 process;
    class C0,C1,C2,C3,C4,C5,C6,C7,C8,C9,C10,C11,C12,C13,C14,C15,C17,OPT_CHECK,DISPATCH decision;
    class DROP drop;
    class END,UDP_IN,TCP_IN,ICMP_IN,IGMP_IN,UNKNOWN startend;
    class FW forward;
```

##  （1）ip4_input()：IPv4报文接收主处理函数
### 函数作用
网卡收到以太网报文、剥离二层头后，驱动将**IP载荷pbuf**传入本函数；完成IP层全部接收逻辑，是IPv4下行入口。
### 阶段拆解
#### 阶段1：基础合法性校验（丢弃畸形报文）
1. IP版本校验：只处理IPv4（`IPH_V(iphdr) ==4`），IPv6直接丢弃
2. IP头长度校验：最小20字节，IP头不能跨pbuf分片，报文总长度不能小于二层实际载荷
3. IP头部校验和校验：无硬件卸载时软件计算`inet_chksum`，校验失败直接丢包
4. 全局缓存：把当前报文源/目的IP存入全局`ip_data`，方便全文件快速读取

#### 阶段2：匹配本机接收网卡netif（判断报文是不是发给本机）
分3种地址场景处理：
1. **组播目的IP**
   - 开启IGMP：仅网卡加入对应组播组才接收，允许IGMP查询报文源IP=0.0.0.0
   - 关闭IGMP：仅网卡配置IP且UP状态接收所有组播
2. **单播/广播目的IP**
   - 优先校验入网卡`inp`是否匹配本机IP/本网段广播（`ip4_input_accept()`）
   - 不匹配则遍历系统所有`netif`寻找匹配网卡
3. **DHCP特殊放行（IP_ACCEPT_LINK_LAYER_ADDRESSING）**
   - 网卡未配置IP、无匹配netif时，UDP目的端口68（DHCP客户端）强制放行，忽略目的IP校验

#### 阶段3：源IP合法性校验（RFC1122规范）
- 源IP不能是本网段广播、任何组播地址；
- 例外：DHCP报文、IGMP查询报文允许源IP=0.0.0.0

#### 阶段4：转发/本地接收分支
1. **无匹配网卡（不是本机报文）**
   - 开启`IP_FORWARD`且不是二层广播报文：调用`ip4_forward()`跨网卡转发
   - 否则直接丢弃报文
2. **匹配本机网卡（本机接收）**
   - 分片判断：IP头MF标记/分片偏移非0 → 进入分片重组`ip4_reass()`
   - IP选项过滤：`IP_OPTIONS_ALLOWED=0`时直接丢弃带IP选项报文（IGMP除外）

#### 阶段5：上层协议分发
1. 先交给RAW原始套接字 `raw_input()`，RAW消费报文则直接结束
2. 未被RAW消费：剥离IP头部20字节，只把传输层载荷传给对应协议：
   - TCP → `tcp_input()`
   - UDP/UDPLITE → `udp_input()`
   - ICMP → `icmp_input()`
   - IGMP → `igmp_input()`
3. 未知传输协议：发送ICMP 协议不可达差错报文

### 关键全局变量 `ip_data`
临时缓存当前正在处理报文的上下文，避免函数层层传参：
- `current_ip4_header`：当前IP头指针
- `current_netif`：本机接收网卡
- `current_input_netif`：报文原始入网卡
- `current_iphdr_src/dest`：报文源、目的IP

## （2）ip4_output() / ip4_output_if()：IPv4报文发送链路
### 分层调用关系
**注意：**

    1. **收包处理**：`ip4_input()` 网卡收包入口，IP头校验、分片重组、转发、上送TCP/UDP/ICMP/IGMP

    2. **发包处理**：`ip4_output()`/`ip4_output_if()` 上层发包入口，路由查找、IP头封装、分片、网卡下发

    3. **路由与转发**：`ip4_route()` 路由查询、`ip4_forward()` 跨网卡IP转发、组播默认网口管理
```
上层应用(TCP/UDP/RAW)
    ↓
ip4_output() 【路由查询入口】
    ↓
ip4_output_if() 【基础IP头封装入口】
    ↓
ip4_output_if_opt() 【带IP选项封装】
    ↓
ip4_output_if_opt_src() 【最终IP头构建、校验和、分片、下发】
```



## 2. ip4_output() 发包总流程（上层TCP/UDP/RAW发包入口）
```mermaid
flowchart TD
    A["上层调用 ip4_output(p, src, dest, ttl, tos, proto)"] --> B["调用 ip4_route_src(src, dest) 路由查询"]
    B --> C{"找到出口netif？"}
    C -- 无路由 --> C1["计数ip.rterr，返回ERR_RTE"]
    C -- 找到网卡netif --> D["调用 ip4_output_if(p, src, dest, ttl, tos, proto, netif)"]
    D --> E["判断dest是否等于LWIP_IP_HDRINCL（上层已自带IP头）"]
    %% 分支1：上层不带IP头，本函数封装IP头
    E -- 无自带IP头 --> F["处理IP选项（IP_OPTIONS_SEND）分配选项空间"]
    F --> F1["pbuf_add_header 预留IP头20字节空间"]
    F1 --> F2["填充IP头：版本4、头长、TOS、TTL、协议、源目的IP、ID自增"]
    F2 --> F3["计算IP头部校验和（硬件卸载/软件inline计算二选一）"]
    %% 分支2：上层自带完整IP头
    E -- 自带IP头 --> G["直接读取报文内IP头，提取目的地址"]
    F3 & G --> H{"目的IP等于出口网卡本机IP / 环回地址？"}
    H -- 本机报文 --> H1["netif_loop_output 环回投递，不走硬件"]
    H -- 外部报文 --> I{"开启IP_FRAG 且报文总长>网卡MTU"}
    I -- 需要分片 --> I1["调用ip4_frag()分片后发送"]
    I -- 无需分片 --> J["调用netif->output 底层网卡发送函数（ARP封装二层）"]
    H1 & I1 & J --> K["返回网卡发送返回值（ERR_OK/ERR_BUF等）"]
```
### （1）ip4_output() 核心逻辑
仅做**路由查询**，无IP封装逻辑：
1. `ip4_route_src(src, dest)` 带源地址的路由钩子查询出口网卡
2. 无路由返回`ERR_RTE`；找到网卡调用`ip4_output_if`

### （2）ip4_output_if_opt_src() 核心发包逻辑（底层实现）
#### 分支A：上层未携带IP头（绝大多数场景，TCP/UDP发包）
1. 分配IP头空间：`pbuf_add_header(p, IP_HLEN)` 在载荷头部腾出20字节IP头空间
2. 填充标准IPv4头部字段：
   - 版本4、头长度、TOS、TTL、传输层协议号
   - 目的IP：入参`dest`
   - 源IP：入参`src`为ANY则自动填充出口网卡IP
   - IP标识ID：全局静态`ip_id`自增，用于分片重组区分报文
   - 分片偏移初始0（不分片）
3. IP校验和计算两种模式：
   - `LWIP_INLINE_IP_CHKSUM=1`：逐字段累加快速计算
   - 关闭inline：调用通用`inet_chksum`计算完整IP头校验和
   - 网卡支持IP校验和硬件卸载：校验和填0，硬件自动填充
4. IP选项处理（`IP_OPTIONS_SEND`）：额外扩展IP头，4字节对齐填充0

#### 分支B：上层传入`LWIP_IP_HDRINCL`（RAW套接字自定义完整IP头）
1. 不封装IP头，直接复用pbuf自带IP头
2. 仅读取目的IP用于路由/二层ARP寻址

#### 发包前处理
1. 环回判断：目的IP为本机网卡IP/环回地址 → `netif_loop_output` 内存环回投递，不走硬件网卡
2. MTU分片判断：报文总长度 > 网卡MTU 且开启`IP_FRAG` → 调用`ip4_frag()`拆分多片IP报文发送
3. 最终下发：调用网卡驱动注册的`netif->output()`，完成ARP查询、封装以太网头，发送到硬件


##  3. ip4_reass() IP分片重组流程（ip4_input分片分支调用，位于ip4_frag.c，ip4.c仅调用）
```mermaid
flowchart TD
    A["ip4_input收到分片报文，调用ip4_reass(p)"] --> B["以{源IP,目的IP,协议,IP-ID}为key查询重组链表"]
    B --> C{"存在同组分片重组缓冲区？"}
    C -- 不存在 --> C1["新建重组控制块reass，加入全局重组链表，设置超时定时器"]
    C -- 存在 --> D["复用已有重组缓冲区"]
    C1 & D --> E["校验分片偏移、载荷长度、是否重叠越界"]
    E -- 分片非法重叠/越界 --> E1["释放整组分片，返回NULL（丢弃）"]
    E -- 合法分片 --> F["将当前分片pbuf挂载到reass分片链表对应偏移位置"]
    F --> G{"是否收到最后一片（MF=0）？"}
    G -- 不是最后一片 --> G1["返回NULL，等待剩余分片"]
    G -- 最后一片 --> H["校验全部分片是否拼接完整无空缺"]
    H -- 存在空缺 --> G1
    H -- 完整 --> I["合并所有分片pbuf为单一完整报文，释放重组控制块"]
    I --> J["返回重组完成的完整IP报文pbuf，回到ip4_input继续上层分发"]
```

## 3.3 ip4_reass() IP分片重组（定义在ip4_frag.c，ip4.c分片路径唯一调用点）
### 触发时机
`ip4_input()`收到IP分片报文（IP头MF置1 或分片偏移>0）时调用。
### 重组核心机制
1. **重组五元组key**：源IP、目的IP、传输协议、IP标识ID，相同五元组判定为同一报文分片
2. 重组缓冲区链表：全局维护`reass`链表，每个节点存储一组分片的全部pbuf、总长度、超时时间
3. 分片合法性检查：
   - 分片偏移*8 + 当前分片载荷长度 不能超过报文总长度
   - 分片载荷不能和已有分片重叠覆盖
4. 拼接逻辑：
   - 非最后分片（MF=1）：存入缓冲区，返回NULL，等待后续分片
   - 收到最后分片（MF=0）：遍历缓冲区检查分片是否无空缺、完整拼接
5. 完整拼接成功：合并所有分片pbuf为单个连续报文，删除重组缓冲区，返回完整IP pbuf给`ip4_input()`
6. 超时清理：lwIP定时任务遍历重组链表，超时未收齐分片直接释放全部缓冲区，防内存泄露

# 四、配套辅助函数简要说明
1. `ip4_route()`：基础路由查询，遍历所有网卡匹配子网掩码；组播优先使用默认组播网卡`ip4_default_multicast_netif`
2. `ip4_route_src()`：支持源地址路由钩子`LWIP_HOOK_IP4_ROUTE_SRC`，自定义复杂路由策略
3. `ip4_forward()`：IP跨网卡转发，TTL减1，TTL=0发送ICMP超时；转发前校验禁止转发广播/组播/本地链路地址；超MTU则分片或发送ICMP需分片差错
4. `ip4_input_accept()`：判断单播/广播报文是否属于当前网卡
5. `ip4_debug_print()`：IP头格式化打印，IP_DEBUG开启时调试输出报文头信息
6. `ip4_set_default_multicast_netif()`：设置组播报文默认发送网卡

# 五、配置宏开关对应本文件功能
| 宏 | 作用 |
|----|------|
| LWIP_IPV4 | 总开关，关闭则整个ip4.c不编译 |
| IP_FORWARD | 开启三层跨网卡路由转发功能 |
| IP_REASSEMBLY | 开启IP分片重组（ip4_reass） |
| IP_FRAG | 开启发送时大报文IP分片（ip4_frag） |
| LWIP_IGMP | 支持组播接收过滤 |
| LWIP_DHCP | DHCP报文特殊放行逻辑 |
| CHECKSUM_GEN_IP / CHECKSUM_CHECK_IP | 软件IP校验和生成/校验 |
| LWIP_INLINE_IP_CHKSUM | 快速inline计算IP头校验和 |
| IP_OPTIONS_SEND / IP_OPTIONS_ALLOWED | 支持IP选项收发 |
| IP_DEBUG | 开启ip4_debug_print调试打印 |

















