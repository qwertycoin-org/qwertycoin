# RPC changes

## ChainLock membership retention

Folded epochs expose archived membership and qualification data through the
EPoSE getters while they remain within the ChainLock carrier retention window
measured from the tip epoch.
After eviction, the getters return empty or unavailable. `get_epose_epoch` and
`get_epose_block_reward` report `Qualification pruned` for a retained epoch
whose qualification keys have been pruned; requests older than the retained
floor report `EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW`.

## QWC-28

`get_vm_state_proof.height` and `get_vm_state_root.height` report the index of
the block whose resulting state is proven. An omitted request height selects
the tip; an explicit zero selects genesis. The Python RPC framework accepts
an optional `height` argument for both calls.

`get_chainlock` with a specific nonzero height is unavailable on restricted
RPC. Both ChainLock and VM state proof requests use the RPC payment check.
