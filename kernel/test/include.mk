src-$(enable-kernel-selftests) += framework.cpp error.cpp cpu.cpp
src-$(enable-kernel-selftests) += cap.cpp int_obj.cpp interrupt.cpp
src-$(enable-kernel-selftests) += ipi.cpp shootdown.cpp
src-$(enable-kernel-selftests) += scheduler.cpp sched_fifo.cpp queue.cpp smp_stress.cpp
src-$(enable-kernel-selftests) += timer.cpp
