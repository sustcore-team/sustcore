/**
 * @file bootstrap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief task bootstrap / preload / spec 装载
 * @version alpha-1.0.0
 * @date 2026-06-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/permission.h>
#include <device/model.h>
#include <elf.h>
#include <exe/elfloader.h>
#include <logger.h>
#include <mem/alloc.h>
#include <sustcore/bootstrap.h>
#include <task/task.h>
#include <vfs/vfs.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace task {
    namespace {
        [[nodiscard]]
        Result<void> create_linux_subsystem_heap(TaskSpec &spec,
                                                 VirAddr heap_start) {
            auto *heap_mem =
                new cap::MemoryPayload(0, false, false, VMA::Growth::FLEXUP);
            auto heap_cap_res = spec.holder->insert_to_free(heap_mem);
            if (!heap_cap_res.has_value()) {
                delete heap_mem;
                propagate_return(heap_cap_res);
            }
            auto heap_res =
                spec.tmm->add_vma(VMA::Type::HEAP, VMA::Growth::FLEXUP,
                                  VirArea(heap_start, heap_start), heap_mem,
                                  VMA::PROT_R | VMA::PROT_W);
            if (!heap_res.has_value()) {
                loggers::TASK::ERROR("无法初始化 POSIX 子系统堆VMA: %d",
                                         heap_res.error());
                propagate_return(heap_res);
            }
            loggers::TASK::INFO(
                "创建POSIX子系统 HEAP VMA: area=[%p,%p) mem=%p memsz=%lu",
                heap_start.addr(), heap_start.addr(), heap_mem, 0UL);
            spec.linuxss_heap_vaddr   = heap_start;
            spec.linuxss_heap_mem_cap = heap_cap_res.value();
            void_return();
        }

        [[nodiscard]]
        bool valid_cap_explain(CapIdx cap_idx, PayloadType cap_type,
                               const char *cap_desc) noexcept {
            return cap_idx != cap::null && cap_idx != cap::error &&
                   cap_type != PayloadType::NONE && cap_desc != nullptr &&
                   cap_desc[0] != '\0';
        }

        [[nodiscard]]
        bool valid_vaddr_explain(VirAddr vaddr,
                                 const char *vaddr_desc) noexcept {
            return vaddr.nonnull() && vaddr_desc != nullptr &&
                   vaddr_desc[0] != '\0';
        }

        [[nodiscard]]
        bool valid_cwd_path(const char *cwd_path) noexcept {
            return cwd_path != nullptr && cwd_path[0] != '\0';
        }

        [[nodiscard]]
        TaskSpec::BootstrapRecordData make_bootstrap_record(
            uint32_t type, const void *payload, size_t payload_size) {
            TaskSpec::BootstrapRecordData record{
                .type  = type,
                .bytes = std::vector<char>(sizeof(bsheader) + payload_size),
            };
            auto *header = reinterpret_cast<bsheader *>(record.bytes.data());
            header->size = record.bytes.size();
            header->type = type;
            if (payload_size != 0) {
                memcpy(record.bytes.data() + sizeof(bsheader), payload,
                       payload_size);
            }
            return record;
        }

    }  // namespace

    Result<CapIdx> TaskManager::preload(CapIdx image_cap, TaskSpec &spec,
                                        LoadPrm &prm) {
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            loggers::TASK::ERROR("创建CHolder失败! 错误码: %s",
                                     to_cstring(create_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto holder = create_res.value();

        auto preload_res = preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
            propagate_return(preload_res);
        }
        return preload_res;
    }

    Result<CapIdx> TaskManager::preload_into(CapIdx image_cap,
                                             cap::CHolder *holder,
                                             TaskSpec &spec, LoadPrm &prm) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto gfp_res = GFP::get_free_page(1);
        if (!gfp_res.has_value()) {
            loggers::TASK::ERROR("无法为程序页表分配物理页");
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto tmm = util::owner(new TaskMemoryManager(gfp_res.value()));

        auto cap_res = holder->lookup(image_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (!cap_res.value()->imply(perm::vfile::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        spec.holder        = holder;
        spec.tmm           = tmm;
        prm.src_path       = "<cap>";
        prm.image_file_cap = image_cap;
        return image_cap;
    }

    Result<void> TaskManager::validate_bootstrap_record(
        const TaskSpec::BootstrapRecordData &record) noexcept {
        if (record.bytes.size() < sizeof(bsheader)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *header = reinterpret_cast<const bsheader *>(record.bytes.data());
        if (header->size != record.bytes.size() || header->type == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapRecordView view{};
        if (!bootstrap_make_view(header, view)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (header->type == boot::TYPE_CAPEXP) {
            BootstrapCapExplainView cap_explain{};
            if (!bootstrap_parse_cap_explain(view, cap_explain) ||
                !valid_cap_explain(cap_explain.cap_idx, cap_explain.cap_type,
                                   cap_explain.cap_desc))
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        } else if (header->type == boot::TYPE_VADDREXP) {
            BootstrapVaddrExplainView vaddr_explain{};
            if (!bootstrap_parse_vaddr_explain(view, vaddr_explain) ||
                !valid_vaddr_explain(vaddr_explain.vaddr,
                                     vaddr_explain.vaddr_desc))
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        } else if (header->type == boot::TYPE_CWDPATH) {
            const char *cwd_path = nullptr;
            if (!bootstrap_parse_cwd_path(view, cwd_path) ||
                !valid_cwd_path(cwd_path))
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        }

        void_return();
    }

    Result<void> TaskManager::append_bootstrap_cap_explain_record(
        TaskSpec &spec, CapIdx cap_idx, PayloadType cap_type, b64 cap_perm,
        const char *cap_desc) {
        if (!valid_cap_explain(cap_idx, cap_type, cap_desc)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapCapExplainPayloadHead head{
            .cap_idx  = cap_idx,
            .cap_type = cap_type,
            .cap_perm = cap_perm,
        };
        std::vector<char> payload(sizeof(head) + strlen(cap_desc) + 1);
        memcpy(payload.data(), &head, sizeof(head));
        memcpy(payload.data() + sizeof(head), cap_desc, strlen(cap_desc) + 1);
        spec.bsargv.push_back(make_bootstrap_record(
            boot::TYPE_CAPEXP, payload.data(), payload.size()));
        void_return();
    }

    Result<void> TaskManager::append_bootstrap_vaddr_explain_record(
        TaskSpec &spec, VirAddr vaddr, const char *vaddr_desc) {
        if (!valid_vaddr_explain(vaddr, vaddr_desc)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapVaddrExplainPayloadHead head{
            .vaddr = vaddr.arith(),
        };
        std::vector<char> payload(sizeof(head) + strlen(vaddr_desc) + 1);
        memcpy(payload.data(), &head, sizeof(head));
        memcpy(payload.data() + sizeof(head), vaddr_desc,
               strlen(vaddr_desc) + 1);
        spec.bsargv.push_back(make_bootstrap_record(
            boot::TYPE_VADDREXP, payload.data(), payload.size()));
        void_return();
    }

    Result<void> TaskManager::load_task_spec(
        CapIdx image_cap, cap::CHolder *holder,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv,
        TaskSpec &spec, LoadPrm &prm) {
        auto preload_res = holder == nullptr
                               ? preload(image_cap, spec, prm)
                               : preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            loggers::TASK::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        auto load_res = loader::elf::load(spec, prm);
        if (!load_res.has_value()) {
            loggers::TASK::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        spec.argv = argv;
        spec.envp = envp;
        spec.bsargv.clear();
        spec.auxv.clear();
        for (const auto &record : bsargv) {
            auto validate_res = validate_bootstrap_record(record);
            propagate(validate_res);
            spec.bsargv.push_back(record);
        }
        void_return();
    }

    Result<void> TaskManager::load_linux_task_spec(
        CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv,
        TaskSpec &spec, LoadPrm &prm) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto preload_res = preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            loggers::TASK::ERROR("预加载POSIX程序失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        std::string interp_path{};
        auto load_linux_res = loader::elf::load_segments(
            spec, prm, true, task::GENERIC_PROCESS_BASE, true, &interp_path);
        if (!load_linux_res.has_value()) {
            loggers::TASK::ERROR("加载POSIX程序失败! 错误码: %s",
                                     to_cstring(load_linux_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        const bool user_program_dyn            = spec.dyn;
        const bool user_program_has_interp     = spec.has_interp;
        const VirAddr user_program_load_base   = spec.load_base;
        const VirAddr user_program_interp_base = spec.interp_base;
        const VirAddr user_program_entrypoint  = spec.program_entrypoint;
        const VirAddr user_program_phdr_vaddr  = spec.phdr_vaddr;
        const size_t user_program_phdr_num     = spec.phdr_num;
        const size_t user_program_phdr_entsize = spec.phdr_entsize;

        VirAddr runtime_entry = user_program_entrypoint;
        if (spec.dyn) {
            if (interp_path.empty()) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            }
            auto interp_cap_res =
                VFS::inst().open(interp_path.c_str(), *holder);
            if (!interp_cap_res.has_value()) {
                loggers::TASK::ERROR(
                    "无法加载 POSIX 解释器! 路径: %s, 错误码 : %s",
                    interp_path.c_str(), to_cstring(interp_cap_res.error()));
                propagate_return(interp_cap_res);
            }
            LoadPrm interp_prm{
                .image_file_cap = interp_cap_res.value(),
                .src_path       = interp_path,
            };
            spec.has_interp      = true;
            spec.interp_base     = task::GENERIC_INTERPRET_BASE;
            auto interp_load_res = loader::elf::load_segments(
                spec, interp_prm, false, task::GENERIC_INTERPRET_BASE, true,
                nullptr);
            if (!interp_load_res.has_value()) {
                loggers::TASK::ERROR("加载POSIX解释器失败! 错误码: %s",
                                         to_cstring(interp_load_res.error()));
                unexpect_return(ErrCode::CREATION_FAILED);
            }
            spec.interp_entrypoint = spec.entrypoint;
            runtime_entry          = spec.interp_entrypoint;
        }

        const VirAddr user_program_interp_entrypoint = spec.interp_entrypoint;

        LoadPrm subsystem_prm{
            .image_file_cap = subsystem_image_cap,
            .src_path       = "<cap>",
        };
        spec.entrypoint           = VirAddr(static_cast<addr_t>(0));
        spec.linuxproc_entrypoint = VirAddr(static_cast<addr_t>(0));
        auto load_subsystem_res =
            loader::elf::load_segments(spec, subsystem_prm, false);
        if (!load_subsystem_res.has_value()) {
            loggers::TASK::ERROR("加载POSIX子系统失败! 错误码: %s",
                                     to_cstring(load_subsystem_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        if (!spec.linuxss_image_end.nonnull()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        VirAddr linuxss_heap_start = spec.linuxss_image_end.page_align_up();
        auto linuxss_heap_res =
            create_linux_subsystem_heap(spec, linuxss_heap_start);
        propagate(linuxss_heap_res);

        spec.dyn                = user_program_dyn;
        spec.has_interp         = user_program_has_interp;
        spec.load_base          = user_program_load_base;
        spec.interp_base        = user_program_interp_base;
        spec.interp_entrypoint  = user_program_interp_entrypoint;
        spec.program_entrypoint = user_program_entrypoint;
        spec.phdr_vaddr         = user_program_phdr_vaddr;
        spec.phdr_num           = user_program_phdr_num;
        spec.phdr_entsize       = user_program_phdr_entsize;

        spec.argv = argv;
        spec.envp = envp;
        spec.linux_execfn.clear();
        if (!spec.linuxss_heap_vaddr.nonnull()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (spec.linuxss_heap_mem_cap == cap::null) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        spec.bsargv.clear();
        for (const auto &record : bsargv) {
            auto validate_res = validate_bootstrap_record(record);
            propagate(validate_res);
            spec.bsargv.push_back(record);
        }
        auto *platform = device::DeviceModel::inst().platform();
        if (platform == nullptr || platform->clock_source() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        spec.linux_execfn =
            prm.src_path.empty() ? std::string{} : std::string(prm.src_path);
        spec.auxv = {
            AT_PHDR,   spec.phdr_vaddr.arith(),
            AT_PHNUM,  spec.phdr_num,
            AT_PHENT,  spec.phdr_entsize,
            AT_PAGESZ, PAGESIZE,
            AT_CLKTCK, platform->clock_source()->frequency().to_hz(),
            AT_ENTRY,  spec.program_entrypoint.arith(),
            AT_BASE,   task::GENERIC_INTERPRET_BASE.arith(),
            AT_UID,    0,
            AT_EUID,   0,
            AT_GID,    0,
            AT_EGID,   0,
            AT_SECURE, 0,
            AT_FLAGS,  0,
        };
        spec.linuxproc_entrypoint = runtime_entry;
        void_return();
    }
}  // namespace task
