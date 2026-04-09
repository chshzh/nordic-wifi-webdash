# Proposal: [Feature Name]

**Date**: YYYY-MM-DD
**Author**: [Your Name]
**Status**: Draft | Under Review | Approved | Rejected
**Related Spec**: [Link to spec if exists]

---

## Problem Statement

What problem does this solve? Why is it needed?

- [ ] User-reported issue
- [ ] Performance/stability improvement
- [ ] New feature request
- [ ] Technical debt reduction

**Context**: Describe the current situation and pain points.

---

## Proposed Solution

High-level description of the solution.

**Key Changes**:
1. Change 1
2. Change 2
3. Change 3

**Architecture Impact**:
- New modules? (list)
- Modified modules? (list)
- New Zbus channels? (describe)
- State machine changes? (describe)

**Board Impact**:
- nRF7002DK: (describe any differences)
- nRF54LM20DK+nRF7002EBII: (describe any differences)

---

## Alternatives Considered

### Alternative 1: [Name]
**Description**: ...
**Pros**: ...
**Cons**: ...
**Rejected because**: ...

### Alternative 2: [Name]
**Description**: ...
**Pros**: ...
**Cons**: ...
**Rejected because**: ...

---

## Technical Details

### Kconfig Changes
```kconfig
CONFIG_NEW_FEATURE=y
CONFIG_NEW_FEATURE_OPTION=value
```

### Code Changes

**New Files**:
- `src/modules/new_feature/new_feature.c`
- `src/modules/new_feature/new_feature.h`
- `src/modules/new_feature/Kconfig.new_feature`
- `src/modules/new_feature/CMakeLists.txt`

**Modified Files**:
- `CMakeLists.txt` — Add new module
- `Kconfig` — Add new_feature Kconfig
- `src/modules/messages.h` — Add new message types
- Other files...

### Memory Impact

| Component | Flash | RAM | Notes |
|-----------|-------|-----|-------|
| New Feature | +X KB | +Y KB | Estimate based on... |
| Total Project | XXX KB | YYY KB | After changes |

**Fits in Budget**: ☑ Yes / ☐ No
**Justification**: ...

### Power Consumption Impact

- [ ] No measurable impact
- [ ] Negligible (<1% average)
- [ ] Moderate (1-5% average)
- [ ] Significant (>5% average)

**Details**: ...

---

## Implementation Plan

### Tasks

1. **[module] Task 1** (Est: X hours)
   - Subtask 1.1
   - Subtask 1.2

2. **[module] Task 2** (Est: Y hours)
   - Subtask 2.1
   - Subtask 2.2

3. **Testing & Documentation** (Est: Z hours)
   - Build test (west build -p both boards)
   - Hardware verification on target board
   - Update specs
   - Update README

**Total Estimate**: XX hours

### Dependencies

- [ ] Dependency 1
- [ ] Dependency 2

### Testing Strategy

**Build Test**:
```bash
# nRF7002DK
west build -p -b nrf7002dk/nrf5340/cpuapp

# nRF54LM20DK
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -- -DSHIELD=nrf7002eb2
```

**Runtime Verification**:
1. Flash firmware
2. Test scenario 1: ...
3. Test scenario 2: ...
4. Verify expected UART log output

**Expected Logs**:
```
[module_name] Feature initialized
[module_name] Expected behavior occurred
```

---

## Risks & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Risk 1 | Low/Med/High | Low/Med/High | Mitigation strategy |
| Risk 2 | Low/Med/High | Low/Med/High | Mitigation strategy |

---

## Non-Goals

What this proposal explicitly does NOT include:

- Non-goal 1
- Non-goal 2

---

## Success Criteria

**Definition of Done**:
- [ ] Code implemented and compiles cleanly for all target boards
- [ ] Runtime testing passes on hardware
- [ ] Memory budget not exceeded
- [ ] Spec created/updated
- [ ] README updated (if user-facing)
- [ ] No regressions in existing features

**Metrics**:
- Metric 1: Target value
- Metric 2: Target value

---

## References

- [NCS Wi-Fi Direct documentation](https://docs.nordicsemi.com/bundle/ncs-3.2.4/page/nrf/protocols/wifi/wifi_direct.html)
- [NCS Documentation](https://docs.nordicsemi.com)
- [Zephyr API](https://docs.zephyrproject.org)
- [wifi-p2p snippet](ncs/v3.2.4/nrf/snippets/wifi-p2p/)
- [WCS-106 Jira: Test P2P with Android Phone](WCS-106.xml)

---

## Discussion Notes

(Add discussion notes here as proposal is reviewed)

**2026-XX-XX**: ...
