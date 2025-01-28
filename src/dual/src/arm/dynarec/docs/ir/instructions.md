
# IR instruction set

## Context Load/Store

### Load from GPR
```
gpr_value<u32> := ldgpr rN, %cpu_mode
```

### Store to GPR
```
stgpr rN, %cpu_mode, new_gpr_value<u32>
```

### Load from CPSR
```
cpsr_value<u32> := ldcpsr
```

### Store to CPSR
```
stcpsr new_cpsr_value<u32>
```

### Load from SPSR
```
spsr_value<u32> := ldspsr %cpu_mode
```

### Store to SPSR
```
stspsr %cpu_mode new_spsr_value<u32>
```

## Flag Management

**TODO**

## Data Processing

```
result_value<u32>                     := add   lhs_value<u32>, rhs_value<u32>
result_value<u32>, hflag_value<hflag> := add.s lhs_value<u32>, rhs_value<u32>
```