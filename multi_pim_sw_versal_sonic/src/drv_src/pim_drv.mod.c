#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x30ff7695, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x11eb121f, "cdev_del" },
	{ 0xac1c4313, "kmalloc_caches" },
	{ 0xd90cd7e6, "cdev_init" },
	{ 0xfd93ee35, "ioremap_wc" },
	{ 0x5c8781db, "pcim_enable_device" },
	{ 0x767ddb02, "set_memory_wc" },
	{ 0x46cf10eb, "cachemode2protval" },
	{ 0x1e6f1b87, "dma_set_mask" },
	{ 0x201394e4, "dma_mmap_attrs" },
	{ 0xcb2f2b52, "boot_cpu_data" },
	{ 0x3d258838, "pci_disable_device" },
	{ 0xb43f9365, "ktime_get" },
	{ 0xc85ac280, "device_destroy" },
	{ 0x3ef73b4d, "pcie_capability_clear_and_set_word" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xf9fd2771, "dma_free_attrs" },
	{ 0xa648e561, "__ubsan_handle_shift_out_of_bounds" },
	{ 0x33a21a09, "pv_ops" },
	{ 0x6eec4dac, "dma_set_coherent_mask" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x172c41f9, "dmaengine_unmap_put" },
	{ 0x25974000, "wait_for_completion" },
	{ 0xb24af53b, "pci_set_master" },
	{ 0x50d1f870, "pgprot_writecombine" },
	{ 0xa22a96f7, "current_task" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0xb99a7c5c, "dmaengine_get_unmap_data" },
	{ 0xa751b1a4, "dma_alloc_attrs" },
	{ 0x91c0bf02, "pci_get_domain_bus_and_slot" },
	{ 0x9befafec, "device_create" },
	{ 0x1d19f77b, "physical_mask" },
	{ 0x646eac6, "cdev_add" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x5b9269bb, "find_vma" },
	{ 0x6e6476e7, "pcie_relaxed_ordering_enabled" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x92997ed8, "_printk" },
	{ 0x461f3cc3, "dma_map_page_attrs" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xfb7e6899, "__dma_request_channel" },
	{ 0x4f00afd3, "kmem_cache_alloc_trace" },
	{ 0xda1dc141, "dma_release_channel" },
	{ 0xdb8acb90, "remap_pfn_range" },
	{ 0x72d79d83, "pgdir_shift" },
	{ 0xedc03953, "iounmap" },
	{ 0x556422b3, "ioremap_cache" },
	{ 0xb3f0559, "class_destroy" },
	{ 0x91607d95, "set_memory_wb" },
	{ 0x608741b5, "__init_swait_queue_head" },
	{ 0xa6257a2f, "complete" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x4a453f53, "iowrite32" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x52ea150d, "__class_create" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xa78af5f3, "ioread32" },
	{ 0xab65ed80, "set_memory_uc" },
	{ 0x9c6febfc, "add_uevent_var" },
	{ 0x8a35b432, "sme_me_mask" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "A3F21347935CAE032671AFB");
