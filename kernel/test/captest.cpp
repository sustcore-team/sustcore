/**
 * @file captest.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力测试函数
 * @version alpha-1.0.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/capability.h>
#include <cap/cspace.h>
#include <cap/cholder.h>
#include <object/csa.h>
#include <object/testobj.h>
#include <perm/csa.h>
#include <perm/testobj.h>
#include <kio.h>

static unsigned g_captest_stage = 0;

static inline unsigned captest_stage(void) { return g_captest_stage; }

static inline void captest_reset(void) { g_captest_stage = 0; }

static inline void captest_stage_begin(const char *title) {
    ++g_captest_stage;
    LOGGER::INFO("========== [Capability Test][阶段%u] %s ==========", g_captest_stage,
                 title);
}

#define TEST_EXPECT(fmt, ...)                                                   \
    LOGGER::INFO("[预期行为][阶段%u] " fmt, captest_stage(), ##__VA_ARGS__)

#define TEST_CHECK(fmt, ...)                                                    \
    LOGGER::INFO("[状态检查][阶段%u] " fmt, captest_stage(), ##__VA_ARGS__)

#define TEST_ACTION(fmt, ...)                                                   \
    LOGGER::INFO("[执行动作][阶段%u] " fmt, captest_stage(), ##__VA_ARGS__)

#define TEST_PASS(fmt, ...)                                                     \
    LOGGER::INFO("[检查通过][阶段%u] " fmt, captest_stage(), ##__VA_ARGS__)

#define TEST_FAIL_RETURN(fmt, ...)                                              \
    do {                                                                        \
        LOGGER::ERROR("[测试失败][阶段%u] " fmt, captest_stage(),               \
                      ##__VA_ARGS__);                                           \
        return;                                                                 \
    } while (0)

void capability_test(void) {
    captest_reset();
    LOGGER::INFO("========== [Capability Test] 开始能力系统测试(CHolder版本) ==========");

    captest_stage_begin("初始化 CHolder 与 CSA");
    TEST_EXPECT("创建两个 CHolder, 分别作为源/目标 CSpace");
    CHolder holder0;
    CHolder holder1;
    CSpace *space0 = &holder0.space();
    CSpace *space1 = &holder1.space();

    auto cap_csa0_opt = holder0.csa();
    auto cap_csa1_opt = holder1.csa();
    if (!cap_csa0_opt.present() || !cap_csa1_opt.present()) {
        TEST_FAIL_RETURN("获取 CHolder 的 CSA 能力失败");
    }
    TEST_PASS("两个 CHolder 的 CSA 能力均已就绪");

    Capability *cap_csa0 = cap_csa0_opt.value();
    Capability *cap_csa1 = cap_csa1_opt.value();
    CSAOperation op0(cap_csa0);
    CSAOperation op1(cap_csa1);

    CapErrCode err;

    captest_stage_begin("创建对象能力并验证初始读值");
    auto idx_obj0_opt = op0.alloc_slot();
    if (!idx_obj0_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_obj0) 失败, 错误码: %s",
                         to_string(idx_obj0_opt.error()));
    }
    CapIdx idx_obj0 = idx_obj0_opt.value();
    TEST_EXPECT("在 holder0 的槽位(%u,%u)创建 TestObject 能力, 初始值应为 12345",
                idx_obj0.group, idx_obj0.slot);
    err = op0.create<TestObject>(idx_obj0, 12345);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建 TestObject 失败, 错误码: %s", to_string(err));
    }

    TEST_CHECK("从 CSpace 取回新建能力并执行 read() 校验初始值");
    auto cap_obj0_opt = space0->get(idx_obj0);
    if (!cap_obj0_opt.present()) {
        TEST_FAIL_RETURN("获取初始对象能力失败");
    }
    TestObjectOperation op_obj0(cap_obj0_opt.value());
    auto read_obj0 = op_obj0.read();
    if (!read_obj0.present() || read_obj0.value() != 12345) {
        TEST_FAIL_RETURN("初始对象读值异常");
    }
    TEST_PASS("对象创建成功, 初始值验证通过");

    captest_stage_begin("Clone 测试");
    auto idx_obj0_clone_opt = op0.alloc_slot();
    if (!idx_obj0_clone_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_obj0_clone) 失败, 错误码: %s",
                         to_string(idx_obj0_clone_opt.error()));
    }
    CapIdx idx_obj0_clone = idx_obj0_clone_opt.value();
    TEST_EXPECT("将(%u,%u) clone 到(%u,%u), 两者应指向同一 payload", idx_obj0.group,
                idx_obj0.slot, idx_obj0_clone.group, idx_obj0_clone.slot);
    err = op0.clone(idx_obj0_clone, space0, idx_obj0);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("clone 失败, 错误码: %s", to_string(err));
    }
    TEST_CHECK("读取 clone 能力, 期望值仍为 12345");
    auto cap_clone_opt = space0->get(idx_obj0_clone);
    if (!cap_clone_opt.present()) {
        TEST_FAIL_RETURN("获取 clone 对象失败");
    }
    TestObjectOperation op_clone(cap_clone_opt.value());
    auto read_clone = op_clone.read();
    if (!read_clone.present() || read_clone.value() != 12345) {
        TEST_FAIL_RETURN("clone 对象读值异常");
    }
    TEST_PASS("clone 能力可访问且读值正确");

    captest_stage_begin("Migrate 跨 CSpace 测试");
    auto idx_obj0_migrated_opt = op1.alloc_slot();
    if (!idx_obj0_migrated_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_obj0_migrated) 失败, 错误码: %s",
                         to_string(idx_obj0_migrated_opt.error()));
    }
    CapIdx idx_obj0_migrated = idx_obj0_migrated_opt.value();
    TEST_EXPECT("将(%u,%u)从 holder0 迁移到 holder1 的(%u,%u), 源槽位应被清空",
                idx_obj0.group, idx_obj0.slot, idx_obj0_migrated.group,
                idx_obj0_migrated.slot);
    err = op1.migrate(idx_obj0_migrated, space0, idx_obj0);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("migrate 失败, 错误码: %s", to_string(err));
    }
    TEST_CHECK("校验源槽位是否已清空");
    if (space0->get(idx_obj0).present()) {
        TEST_FAIL_RETURN("migrate 后源槽位未清空");
    }

    TEST_CHECK("校验目标槽位是否存在并可读");
    auto cap_migrated_opt = space1->get(idx_obj0_migrated);
    if (!cap_migrated_opt.present()) {
        TEST_FAIL_RETURN("migrate 后目标槽位为空");
    }
    Capability *cap_migrated = cap_migrated_opt.value();
    TestObjectOperation op_migrated(cap_migrated);
    auto read_migrated = op_migrated.read();
    if (!read_migrated.present() || read_migrated.value() != 12345) {
        TEST_FAIL_RETURN("migrate 后对象读值异常");
    }
    TEST_PASS("migrate 完成: 源槽位已清空, 目标槽位读值正确");

    captest_stage_begin("降权测试(READ -> NONE)");
    PermissionBits read_only(perm::testobj::READ, PayloadType::TEST_OBJECT);
    err = cap_migrated->downgrade(read_only);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("降权到 READ 失败, 错误码: %s", to_string(err));
    }

    TEST_EXPECT("当前仅保留 READ 权限, 接下来调用 increase() 应触发 \"权限不足\" 错误日志");
    op_migrated.increase();

    TEST_CHECK("increase() 被拒绝后, 继续读取对象值以确认未发生修改");
    auto read_after_increase = op_migrated.read();
    if (!read_after_increase.present() || read_after_increase.value() != 12345)
    {
        TEST_FAIL_RETURN("READ-only 下 increase 仍然生效");
    }
    TEST_PASS("READ-only 下写操作已被拒绝, 对象值保持不变");

    PermissionBits no_perm(0, PayloadType::TEST_OBJECT);
    captest_stage_begin("RecvSpace 接收测试");
    TEST_EXPECT("RecvSpace 仅允许接收来自预设 sender 的迁移能力");

    auto idx_recv_src_opt = op0.alloc_slot();
    if (!idx_recv_src_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_recv_src) 失败, 错误码: %s",
                         to_string(idx_recv_src_opt.error()));
    }
    CapIdx idx_recv_src = idx_recv_src_opt.value();

    err = op0.create<TestObject>(idx_recv_src, 24680);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建 RecvSpace 源能力失败, 错误码: %s", to_string(err));
    }

    auto cap_recv_src_opt = space0->get(idx_recv_src);
    if (!cap_recv_src_opt.present()) {
        TEST_FAIL_RETURN("获取 RecvSpace 源能力失败");
    }
    Capability *cap_recv_src = cap_recv_src_opt.value();

    CapIdx idx_recv_dst(SpaceType::RECV, idx_recv_src.group, 0);

    TEST_CHECK("未设置 sender 时接收应失败");
    err = holder1.recv_space().migrate(idx_recv_dst, cap_recv_src);
    if (err != CapErrCode::INVALID_INDEX) {
        TEST_FAIL_RETURN("未设置 sender 时 RecvSpace 接收未失败");
    }

    TEST_CHECK("设置错误 sender 时接收应失败");
    holder1.recv_space().set_sender(idx_recv_dst.group, holder1.cholder_id);
    err = holder1.recv_space().migrate(idx_recv_dst, cap_recv_src);
    if (err != CapErrCode::INVALID_INDEX) {
        TEST_FAIL_RETURN("错误 sender 时 RecvSpace 接收未失败");
    }

    TEST_CHECK("设置正确 sender 后接收应成功");
    holder1.recv_space().set_sender(idx_recv_dst.group, holder0.cholder_id);
    err = holder1.recv_space().migrate(idx_recv_dst, cap_recv_src);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("正确 sender 时 RecvSpace 接收失败, 错误码: %s",
                         to_string(err));
    }

    auto cap_recv_dst_opt = holder1.access(idx_recv_dst);
    if (!cap_recv_dst_opt.present()) {
        TEST_FAIL_RETURN("无法通过 CHolder::access 访问 RecvSpace 能力");
    }
    TestObjectOperation op_recv_dst(cap_recv_dst_opt.value());
    auto read_recv_dst = op_recv_dst.read();
    if (!read_recv_dst.present() || read_recv_dst.value() != 24680) {
        TEST_FAIL_RETURN("RecvSpace 目标能力读值异常");
    }
    TEST_PASS("RecvSpace sender 校验与 access(RECV) 访问均符合预期");
    err = cap_migrated->downgrade(no_perm);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("降权到 NONE 失败, 错误码: %s", to_string(err));
    }

    TEST_EXPECT("当前权限为 NONE, 接下来 read() 应触发 \"权限不足\" 并返回 INSUFFICIENT_PERMISSIONS");
    auto final_read = op_migrated.read();
    if (final_read.present() ||
        final_read.error() != CapErrCode::INSUFFICIENT_PERMISSIONS)
    {
        TEST_FAIL_RETURN("NONE 权限下读取行为异常");
    }
    TEST_PASS("NONE 权限下 read() 已按预期被拒绝");

    captest_stage_begin("Revoke 子树清理测试");
    TEST_EXPECT("构造一棵能力派生树, 删除中间节点后应清理其全部后代, 保留根与兄弟节点");

    auto idx_root_opt = op0.alloc_slot();
    if (!idx_root_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_root) 失败, 错误码: %s",
                         to_string(idx_root_opt.error()));
    }
    CapIdx idx_root = idx_root_opt.value();

    err = op0.create<TestObject>(idx_root, 999);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建 revoke 根节点失败, 错误码: %s", to_string(err));
    }

    auto idx_l1_keep_opt = op0.alloc_slot();
    if (!idx_l1_keep_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_l1_keep) 失败, 错误码: %s",
                         to_string(idx_l1_keep_opt.error()));
    }
    CapIdx idx_l1_keep = idx_l1_keep_opt.value();

    err = op0.clone(idx_l1_keep, space0, idx_root);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建保留兄弟节点失败, 错误码: %s", to_string(err));
    }

    auto idx_l1_revoke_opt = op0.alloc_slot();
    if (!idx_l1_revoke_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_l1_revoke) 失败, 错误码: %s",
                         to_string(idx_l1_revoke_opt.error()));
    }
    CapIdx idx_l1_revoke = idx_l1_revoke_opt.value();

    err = op0.clone(idx_l1_revoke, space0, idx_root);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建待撤销分支失败, 错误码: %s", to_string(err));
    }

    auto idx_l2_revoke_opt = op0.alloc_slot();
    if (!idx_l2_revoke_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_l2_revoke) 失败, 错误码: %s",
                         to_string(idx_l2_revoke_opt.error()));
    }
    CapIdx idx_l2_revoke = idx_l2_revoke_opt.value();

    err = op0.clone(idx_l2_revoke, space0, idx_l1_revoke);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建二级分支失败, 错误码: %s", to_string(err));
    }

    auto idx_l3_revoke_opt = op1.alloc_slot();
    if (!idx_l3_revoke_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_l3_revoke) 失败, 错误码: %s",
                         to_string(idx_l3_revoke_opt.error()));
    }
    CapIdx idx_l3_revoke = idx_l3_revoke_opt.value();

    err = op1.clone(idx_l3_revoke, space0, idx_l2_revoke);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建跨 Space 三级分支失败, 错误码: %s", to_string(err));
    }

    TEST_CHECK("删除待撤销分支根节点(%u,%u), 触发子树级联清理",
               idx_l1_revoke.group, idx_l1_revoke.slot);
    err = op0.remove(idx_l1_revoke);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("revoke 删除失败, 错误码: %s", to_string(err));
    }

    TEST_CHECK("校验被撤销分支后代是否全部删除");
    if (space0->get(idx_l2_revoke).present() ||
        space1->get(idx_l3_revoke).present())
    {
        TEST_FAIL_RETURN("revoke 未正确清理后代");
    }
    TEST_CHECK("校验根节点与未撤销兄弟节点是否仍然存在");
    if (!space0->get(idx_root).present() || !space0->get(idx_l1_keep).present())
    {
        TEST_FAIL_RETURN("revoke 误删根节点或兄弟节点");
    }
    TEST_PASS("revoke 仅清理目标子树, 未影响非目标分支");

    captest_stage_begin("服务调用握手测试");
    TEST_EXPECT("Server 开放 RecvSpace 指定分组, Client 写入降权 CSA 子集与请求对象, Server 读取请求后向 Client 写回响应对象");

    constexpr b16 handshake_group = 900;

    TEST_CHECK("Client 克隆自身 CSA 能力并降权为子集权限");
    auto idx_client_csa_subset_opt = op0.alloc_slot();
    if (!idx_client_csa_subset_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_client_csa_subset) 失败, 错误码: %s",
                         to_string(idx_client_csa_subset_opt.error()));
    }
    CapIdx idx_client_csa_subset = idx_client_csa_subset_opt.value();

    err = op0.clone(idx_client_csa_subset, space0, holder0.csa_idx());
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("克隆 Client CSA 子能力失败, 错误码: %s", to_string(err));
    }

    auto cap_client_csa_subset_opt = space0->get(idx_client_csa_subset);
    if (!cap_client_csa_subset_opt.present()) {
        TEST_FAIL_RETURN("获取 Client CSA 子能力失败");
    }
    Capability *cap_client_csa_subset = cap_client_csa_subset_opt.value();

    b64 csa_subset_bitmap[perm::csa::BITMAP_SIZE] = {0};
    const size_t subset_offset = perm::csa::bitmap_offset(handshake_group);
    csa_subset_bitmap[subset_offset / 64] |=
        (static_cast<b64>(perm::csa::SLOT_INSERT) << (subset_offset % 64));

    PermissionBits csa_subset_perm(perm::csa::ALLOC,
                                   csa_subset_bitmap,
                                   PayloadType::CSPACE_ACCESSOR);
    err = cap_client_csa_subset->downgrade(csa_subset_perm);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("降权 Client CSA 子能力失败, 错误码: %s", to_string(err));
    }

    TEST_CHECK("Client 创建请求 TestObject");
    auto idx_client_req_opt = op0.alloc_slot();
    if (!idx_client_req_opt.present()) {
        TEST_FAIL_RETURN("alloc_slot(idx_client_req) 失败, 错误码: %s",
                         to_string(idx_client_req_opt.error()));
    }
    CapIdx idx_client_req = idx_client_req_opt.value();
    err = op0.create<TestObject>(idx_client_req, 13579);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("创建请求 TestObject 失败, 错误码: %s", to_string(err));
    }

    TEST_CHECK("Server 开放 RecvSpace 分组并接收 Client 迁移过来的能力");
    CapIdx idx_server_recv_csa(SpaceType::RECV, handshake_group, 0);
    CapIdx idx_server_recv_req(SpaceType::RECV, handshake_group, 1);
    holder1.recv_space().set_sender(handshake_group, holder0.cholder_id);

    err = holder1.recv_space().migrate(idx_server_recv_csa, cap_client_csa_subset);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("Server 接收 Client CSA 子能力失败, 错误码: %s",
                         to_string(err));
    }

    auto cap_client_req_opt = space0->get(idx_client_req);
    if (!cap_client_req_opt.present()) {
        TEST_FAIL_RETURN("获取 Client 请求能力失败");
    }
    err = holder1.recv_space().migrate(idx_server_recv_req, cap_client_req_opt.value());
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("Server 接收 Client 请求对象失败, 错误码: %s",
                         to_string(err));
    }

    TEST_CHECK("Server 读取请求对象能力并校验可读");
    TEST_ACTION("Server 通过 RecvSpace 索引访问请求对象能力");
    auto cap_server_recv_req_opt = holder1.access(idx_server_recv_req);
    if (!cap_server_recv_req_opt.present()) {
        TEST_FAIL_RETURN("Server 无法访问接收的请求对象能力");
    }
    TEST_ACTION("Server 构造 TestObjectOperation 并执行 read()");
    TestObjectOperation op_server_req(cap_server_recv_req_opt.value());
    auto req_value_opt = op_server_req.read();
    if (!req_value_opt.present()) {
        TEST_FAIL_RETURN("Server 读取请求对象失败, 错误码: %s",
                         to_string(req_value_opt.error()));
    }
    int req_value = req_value_opt.value();
    TEST_CHECK("Server 成功读取请求值: %d", req_value);

    TEST_CHECK("Server 读取 CSA 子能力并准备写回 Client");
    TEST_ACTION("Server 通过 RecvSpace 索引访问 CSA 子能力");
    auto cap_server_recv_csa_opt = holder1.access(idx_server_recv_csa);
    if (!cap_server_recv_csa_opt.present()) {
        TEST_FAIL_RETURN("Server 无法访问接收的 CSA 子能力");
    }
    TEST_ACTION("Server 构造 CSAOperation, 申请 Client 回包槽位");
    CSAOperation op_server_to_client(cap_server_recv_csa_opt.value());
    auto idx_client_reply_opt = op_server_to_client.alloc_slot();
    if (!idx_client_reply_opt.present()) {
        TEST_FAIL_RETURN("Server 通过 CSA 子能力 alloc_slot 失败, 错误码: %s",
                         to_string(idx_client_reply_opt.error()));
    }
    CapIdx idx_client_reply = idx_client_reply_opt.value();
    if (idx_client_reply.group != handshake_group) {
        TEST_FAIL_RETURN("CSA 子能力越权: 分配到未授权分组(%u)",
                         idx_client_reply.group);
    }
    TEST_CHECK("Server 分配回包槽位成功: (%u,%u)", idx_client_reply.group,
               idx_client_reply.slot);

    const int reply_value = req_value + 100;
    TEST_ACTION("Server 向 Client 回包槽位写入响应对象, 值=%d", reply_value);
    err = op_server_to_client.create<TestObject>(idx_client_reply, reply_value);
    if (err != CapErrCode::SUCCESS) {
        TEST_FAIL_RETURN("Server 向 Client 写回响应对象失败, 错误码: %s",
                         to_string(err));
    }
    TEST_CHECK("Server 写回响应对象成功");

    TEST_CHECK("Client 获取 Server 写回的响应对象能力");
    TEST_ACTION("Client 在主 CSpace 读取响应对象能力");
    auto cap_client_reply_opt = space0->get(idx_client_reply);
    if (!cap_client_reply_opt.present()) {
        TEST_FAIL_RETURN("Client 无法获取响应对象能力");
    }
    TEST_ACTION("Client 构造 TestObjectOperation 并执行 read() 验证响应值");
    TestObjectOperation op_client_reply(cap_client_reply_opt.value());
    auto reply_read_opt = op_client_reply.read();
    TEST_CHECK("Client 校验响应值应为 %d", reply_value);
    if (!reply_read_opt.present() || reply_read_opt.value() != reply_value) {
        TEST_FAIL_RETURN("Client 读取响应对象异常");
    }
    TEST_PASS("Client 响应对象读值正确: %d", reply_read_opt.value());
    TEST_PASS("服务调用握手成功: Server 正确读取请求并向 Client 回写响应对象");

    captest_stage_begin("测试完成");
    TEST_PASS("CHolder 版本能力系统测试通过");
}