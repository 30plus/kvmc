#ifndef KVM__SYMBOL_H
#define KVM__SYMBOL_H

#include <stddef.h>
#include <string.h>

struct kvm;

#define SYMBOL_DEFAULT_UNKNOWN "<unknown>"

int symbol_init(struct kvm *kvm);
int symbol_exit(struct kvm *kvm);
char *symbol_lookup(struct kvm *kvm, unsigned long addr, char *sym, size_t size);

#endif /* KVM__SYMBOL_H */
