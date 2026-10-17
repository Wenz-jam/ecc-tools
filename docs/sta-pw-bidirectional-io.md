# iSTA / iPW 双向 IO 建图方案

## 状态与目标

本文是待实施方案，不表示双向 graph 已经完成。当前改动只撤销两个工具在 Interface 中删除 PAD 外侧 signal net、顶层 port 和 PAD pin 的处理，并保留独立的 iSTA 电源地过滤。

目标是让带 IO cell 的设计保留原来的整芯片 SDC 和 VCD，正确分析输入、输出及模式相关的双向路径。iSTA 与 iPW 继续独立，不新增跨工具调用，不改变 Tcl/Python API。

## 为什么不能沿用布线跳过规则

iRT 跳过无需布线的顶层 port 与 PAD 连接，不代表该连接没有时序或活动传播意义。删除它会使 `get_ports` 失效，切断外部时钟，并丢失输入到达时间、输出负载和 VCD 注解。报告筛选也不能通过删除这些对象实现。

IO cell 的 LEF 可以是物理黑盒，但其 Liberty 仍可包含输入和输出时序弧。不能仅依据 LEF `CLASS PAD`、pin 名称或一对一连接就跳过分析。

## 已确认的代码限制

| 位置 | 当前限制 | 需要调整 |
| --- | --- | --- |
| 两个工具的 `GraphBuilder::buildInoutPinDirectionByGraph()` | 将每个 INOUT 强制推导为单一方向，不能推导就终止 | 保留真实双向角色，不通过删除 pin 或任意选择方向处理 |
| `Pin`、`Arc`、`Database` | 邻接关系和传播状态主要以 pin 名为 key | 分开物理 pin 身份和图节点身份，同一个双向 pin 的两种角色不能共用传播状态 |
| iSTA `Net`、`DelayCalculator` | 部分调用依赖唯一 `driver_pin`，寄生负载也从它选根 | 显式选择当前 driver，按该 driver 求负载和 RC 响应 |
| Liberty parser 的 `LibPort`、两个工具的 `TimingCellPort` | 有方向和 `function`，没有保存 pin 的 `three_state` 表达式 | 补齐解析及 wrap，不按 OE、IE 等名字猜功能 |
| 两个工具的 case analysis | iSTA 在传播阶段应用；iPW 在建图后段应用 | 在最终活动弧选择和拓扑排序之前处理模式，不能先单向化再补模式 |
| iPW `PowerWorkspace`、功耗汇总 | 活动和缓存按物理 pin 索引，功耗按 instance/net 汇总 | 新增图节点不能复制物理实例、重复计算电容或重复累加功耗 |
| iPW `SdcCommand` | 尚不支持复现 SDC 所使用的 `get_fanin` 查询 | 单独补齐执行原始脚本所必需的对象查询，不能忽略错误后宣称约束完整 |

## 数据模型

1. 保留原始 `Pin`、`Net`、`Instance`，名称不变。SDC、VCD、SPEF、RPT 和 SDF 继续对应真实对象，禁止在 pin 名后拼虚构后缀来模拟第二个引脚。
2. 增加独立的图节点身份及其到物理 pin 的映射。普通单向 pin 只需一个对应角色的节点；双向 pin 同时有 load 节点和 driver 节点。顶层 input port 是设计的 driver，顶层 output port 是设计的 load。
3. 图节点保存独立的传播状态和邻接关系；边记录源、目标图节点，同时能够反查物理 pin 和原来的 Liberty arc。
4. 跨模块使用的图数据放各工具自身的 `source/data_manager/advance`；建图临时数据放 `graph_builder/gb_data_manager`。不复制实例和 RC 电容，不让 module 持有外部 Liberty 对象。

这里的两节点表示参考 OpenSTA 的 `makePinVertices()` 和 `makeWireEdge()`。OpenSTA 将双向引脚的驱动节点单独保存，net edge 从驱动节点连到负载节点。[Graph.cc](https://github.com/parallaxsw/OpenSTA/blob/master/graph/Graph.cc)

## 建图与模式

```text
输入：top port(driver) -> PAD(load) -> IO cell 接收输出 -> core
输出：core -> IO cell 数据输入 -> PAD(driver) -> top port(load)
```

- cell arc 按 Liberty 连接相应节点，不能把输入方向的 PAD 到 core 路径和输出方向的 core 到 PAD 路径混成一个状态。
- net 保存候选驱动及负载角色，并排除同一物理 pin 自己到自己的 net edge。普通多输出短接仍须诊断；双向或三态端口具有多个候选驱动不等于它们同时有效。
- 先建立角色关系，再传播网表常量和 SDC case analysis，计算活动弧，最后进行环检查和拓扑排序。不能通过先强制单驱动才能解析模式的循环依赖来建图。
- `three_state` 是高阻条件，表达式为真表示输出关闭。可在内部保存该条件，或统一转换成驱动使能表达式，但不能把极性弄反。OpenSTA 在读取时对该表达式取反，保存为 enable。[LibertyReader.cc](https://github.com/parallaxsw/OpenSTA/blob/master/liberty/LibertyReader.cc)
- 结合 `function`、`when`、三态条件判断有效路径。已知关闭的输出不能作为有效驱动；模式未知时保留可能路径及不确定性，不将未知默认成关闭，不随机选择一个 driver。
- 区分数据弧、三态使能弧以及双向回读路径。不能用通用断环随意删除输入或输出方向。OpenSTA 对双向实例回读和时钟遍历有专门限制，不能只复制“两节点”而忽略搜索规则。[SearchPred.cc](https://github.com/parallaxsw/OpenSTA/blob/master/search/SearchPred.cc)
- 电源地不作为信号时序节点，但供电连接给逻辑控制 pin 带来的常量值仍需保留；不要因为过滤供电网而丢失三态控制的常量信息。

## 输入边界与解析

Liberty parser 负责把 `three_state` 保存到库对象，包含 bus 属性继承；Interface 再将它 wrap 为各工具自己的 `LogicExpression`。这是预计需要修改 parser 的最小范围，算法模块不直接调用 parser，也不重新扫描 Liberty 文本。

SDC 继续绑定顶层 port，输入 delay、输入 transition/drive 对应输入传播角色，输出 delay/load 对应输出传播角色。时钟沿真实的接收路径进入 core。不能把外部负载直接平移到 core pin，也不能忽略空对象错误。

iPW 的完整 SDC 兼容性是独立的前置问题，不能用本次 PAD 输入恢复来代替。需要保留功耗用到的时钟、模式及电气约束，并支持其对象查询；不因此把 iSTA 的 setup/hold 分析模块重新复制进 iPW。

## 后续模块适配

| 工具/模块 | 职责 |
| --- | --- |
| iSTA `ClockPropagator` | 从端口 driver 传播时钟，通过 PAD load 和 IO 接收弧到 core；避免双向回读伪时钟路径 |
| iSTA `TimingPropagator` / `TimingAnalyzer` | 以图节点区分 arrival、slew、required、前驱及起终点；不把双向 pin 的两方向状态互相覆盖 |
| 两个工具的 `DelayCalculator` | 以实际 driver 为 RC 根，缓存区分 net、driver、分析条件；同一物理电容只算一次 |
| iPW `SignalPropagator` / `PowerPropagator` | 保留顶层 VCD 注解，分别传播接收和驱动状态；保持物理 pin 索引与图节点索引分离 |
| iPW `PowerAnalyzer` | 内部功耗按有效弧和活动计算；输出切换功耗不能把外部驱动的活动无条件算到高阻 IO 输出上，也不能因两个节点重复累加；泄漏仍按物理实例和模式计算 |
| RPT / SDF / Liberty 导出 | 通过映射输出原始 pin 名，不能泄漏内部节点名；保留真实 IO 路径，报告筛选与计算连通分离 |

iPW 的未知三态模式需要单独定义功耗归属策略：仅有 PAD 的翻转率不能推断是谁驱动。优先使用使能信号活动或明确的分析模式；缺少信息时必须说明估计或未覆盖的部分，不把它当成精确功耗。

## 实施顺序

1. 撤销 PAD 输入过滤，修正说明与回归测试。此阶段只恢复输入，不宣称完整设计已跑通。
2. 补齐三态表达式的数据链和图节点角色，在小型输入、输出、双向电路上验证结构及模式。
3. 完整适配 iSTA 的时钟、数据、required 传播和延迟计算，再验证原始设计、原始 SDC。
4. 在 iPW 独立实现同样的角色语义，适配 VCD、缓存、活动传播和功耗归属，不调用 iSTA 内部模块。
5. 回归普通单向设计、no-SPEF/with-SPEF、报告及导出格式，确认没有双计数和新增假路径。

## 验收用例

- 保留 `get_ports` 的匹配、原端口 clock source、输入输出 delay、load，以及双向端口 VCD 活动值。
- 输入模式能从顶层 port 到达 core 寄存器；输出模式能从 core 寄存器到达顶层 port。
- 关闭某方向后，该方向不传播；未知模式不被错误折叠或删除。
- 两个不同 IO 实例可具有不同模式；cell/pin 改名不改变结果。
- 常量控制、`set_case_analysis`、条件弧、双向回读和真正多驱动分别覆盖，不能只检查“不崩溃”。
- 同一双向 net 从不同 driver 求得各自的 RC 响应，物理负载不重复累计。
- iPW 外部驱动、内部驱动、高阻模式分别检查活动来源、slew 及功耗归属；实例数、pin 数、net 数不因图拆分翻倍。
- 普通单向设计的已有结果不因结构变化而改变；RPT/SDF 仍使用原设计名称。
