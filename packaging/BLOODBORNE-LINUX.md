# Bloodborne Linux port — AMD GCN / RADV requirements

The Linux port requires Vulkan 1.3. An AMD Radeon R9 290X is a GCN/GFX7 GPU. Mesa's RADV driver documents Vulkan 1.3 support for GFX6–GFX7 GPUs.

## Kernel driver

RADV requires the Linux `amdgpu` kernel driver. Older kernels may bind GFX6/GFX7 cards to the legacy `radeon` driver instead. Check the active driver with:

```sh
lspci -k -s "$(lspci -d 1002:67b0 | head -n1 | cut -d' ' -f1)"
```

Look for:

```
Kernel driver in use: amdgpu
```

On older kernels where the R9 290X is still bound to `radeon`, enable the GCN support in `amdgpu` with these kernel parameters, then reboot:

```
radeon.si_support=0 radeon.cik_support=0 amdgpu.si_support=1 amdgpu.cik_support=1
```

Do not add these parameters blindly if the card is already using `amdgpu`.

## Verify Vulkan before launching

Install `vulkan-tools` and run:

```sh
vulkaninfo --summary
```

Confirm that the R9 290X is listed and reports Vulkan API version 1.3 or newer. The port also needs these device extensions:

- `VK_KHR_swapchain`
- `VK_KHR_push_descriptor`
- `VK_EXT_vertex_attribute_divisor`
- `VK_EXT_robustness_2`

The `robustness2` feature set must include `robustBufferAccess2`, `robustImageAccess2` and `nullDescriptor`. If these are missing, the renderer will reject the device.

## AppImage notes

The Bloodborne AppImage bundles the RADV userspace Vulkan ICD, but it cannot bundle or replace the host kernel's `amdgpu` driver. The host still needs a working DRM/KMS setup and the correct kernel module. The AppImage data directory defaults to `~/.local/share/bbport`; saves, shader cache, `bbport.ini`, mods and patches stay outside the AppImage.
