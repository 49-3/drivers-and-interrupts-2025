# 🔧 GUIDE DE CORRECTION - Phase 1 (Obligatoire)

## 1. CORRIGER LE FORMAT event_to_str() 

**Fichier:** `utils.c`  
**Ligne:** ~190  
**Priorité:** 🔴 CRITIQUE

### Avant:
```c
sprintf(output_str, "[%d:%d:%d] %s(0X%X) Pressed\n", 
    time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec, 
    event.name, event.scan_code);
```

### Après:
```c
snprintf(output_str, 256, "%02d:%02d:%02d %s(%d) %s\n",
    time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec,
    event.name, event.scan_code,
    event.is_pressed ? "Pressed" : "Released");
```

**Détails:**
- `%02d` → Force 2 chiffres avec padding zéro
- `%d` → Code décimal au lieu de hex
- `event.is_pressed ? ... : ...` → Dynamic "Pressed" vs "Released"
- `snprintf()` → Limite à 256 bytes (sécurité)
- Format: `HH:MM:SS Name(code) State` ✓

---

## 2. SÉCURISER L'ALLOCATION MÉMOIRE

**Fichier:** `device.c`  
**Fonction:** `ft_module_keyboard_read()`  
**Ligne:** ~50  
**Priorité:** 🔴 CRITIQUE

### Avant:
```c
output_str = kmalloc(69420 * 42, GFP_KERNEL);
output_str[0] = 0;

// ... remplir buffer ...

did_not_cpy = copy_to_user(buff, output_str, strlen(output_str));
```

### Après:
```c
#define READ_BUFFER_MAX 8192

output_str = kmalloc(READ_BUFFER_MAX, GFP_KERNEL);
if (!output_str) {
    ft_warn("Failed to allocate read buffer");
    return -ENOMEM;
}
output_str[0] = '\0';

// Remplir le buffer avec limite
size_t current_len = 0;
head_ptr = g_driver->events_head->list.next;
while (head_ptr != &(g_driver->events_head->list) && 
       current_len < READ_BUFFER_MAX - 256) {
    entry = list_entry(head_ptr, struct event_struct, list);
    temp_str = event_to_str(*entry);
    
    // Vérifier overflow
    size_t needed = strlen(temp_str);
    if (current_len + needed >= READ_BUFFER_MAX) {
        kfree(temp_str);
        break;  // Stop si pas assez de place
    }
    
    strncat(output_str, temp_str, READ_BUFFER_MAX - current_len - 1);
    current_len = strlen(output_str);
    kfree(temp_str);
    head_ptr = head_ptr->next;
}
spin_unlock(&devfile_io_spinlock);

// Gérer offset correctement
if (*offset >= current_len) {
    kfree(output_str);
    return 0;  // EOF
}

size_t remaining = current_len - *offset;
size_t to_copy = remaining < *buff_size ? remaining : *buff_size;

int did_not_cpy = copy_to_user(buff, output_str + *offset, to_copy);
if (did_not_cpy) {
    ft_warn("copy_to_user failed");
    kfree(output_str);
    return -EFAULT;
}

*offset += (to_copy - did_not_cpy);
kfree(output_str);
return to_copy - did_not_cpy;
```

**Changements clés:**
- ✅ Vérifier retour `kmalloc()`
- ✅ Limiter allocation à 8 KB max
- ✅ Utiliser `strncat()` au lieu de `strcpy()`
- ✅ Vérifier overflow avant concat
- ✅ Gérer offset correctement (lectures fragmentées)
- ✅ Vérifier retour `copy_to_user()`

---

## 3. AJOUTER SPINLOCK SUR list_add_tail()

**Fichier:** `interrupt.c`  
**Fonction:** `read_key()`  
**Ligne:** ~35  
**Priorité:** 🔴 CRITIQUE

### Avant:
```c
void read_key(struct work_struct *workqueue) {
    // ...
    event = ft_generate_event(*q_data, scancode);
    
    // ⚠️ PAS DE PROTECTION !
    list_add_tail(&(event->list), &(q_data->driver.events_head->list));
    
    // post processing
    spin_lock(&q_data_spinlock);
    // ...
}
```

### Après:
```c
void read_key(struct work_struct *workqueue) {
    // ...
    event = ft_generate_event(*q_data, scancode);
    
    // ✅ PROTECTION AVEC SPINLOCK
    spin_lock(&devfile_io_spinlock);
    list_add_tail(&(event->list), &(q_data->driver.events_head->list));
    g_driver->total_events++;  // Bonus: compter événements
    spin_unlock(&devfile_io_spinlock);
    
    // post processing
    spin_lock(&q_data_spinlock);
    // ...
}
```

**Pourquoi:**
- Deux IRQ simultanées pourraient corrompre la liste
- `devfile_io_spinlock` est déjà utilisé dans device.c
- Utiliser le MÊME spinlock partout pour la liste

---

## 4. IMPLÉMENTER SMART LOGGING DANS cleanup_module()

**Fichier:** `main.c`  
**Fonction:** `cleanup_module()`  
**Ligne:** ~160  
**Priorité:** 🟡 IMPORTANT

### Avant:
```c
void cleanup_module(void) {
    ft_log("Cleaning up module");
    ft_log_tmpfile();  // Écrit tout dans /tmp
    ft_deregister_interrupt();
    ft_destroy_misc_device();
    ft_deregister_usb();
    ft_free_driver(g_driver);
}
```

### Après:
```c
void cleanup_module(void) {
    ft_log("Cleaning up module");
    
    // Log stats AVANT de supprimer
    ft_log_tmpfile_with_stats();  // NOUVELLE FONCTION
    
    // Cleanup
    ft_deregister_interrupt();
    ft_destroy_misc_device();
    ft_deregister_usb();
    ft_free_driver(g_driver);
    
    ft_log("Module cleanup complete");
}
```

**Nouvelle fonction à ajouter dans `tmpfile.c`:**
```c
void ft_log_tmpfile_with_stats(void) {
    struct file *file;
    struct list_head *head_ptr;
    struct event_struct *entry;
    char *output_str;
    char *stats_str;
    int total_alpha = 0;
    int total_events = 0;

    // Créer header
    output_str = kmalloc(8192, GFP_KERNEL);
    stats_str = kmalloc(2048, GFP_KERNEL);
    if (!output_str || !stats_str) {
        ft_warn("Memory allocation failed for stats");
        return;
    }

    // Compter événements alphanumériques
    head_ptr = g_driver->events_head->list.next;
    while (head_ptr != &(g_driver->events_head->list)) {
        entry = list_entry(head_ptr, struct event_struct, list);
        total_events++;
        if (entry->is_pressed && entry->ascii_value > 32 && entry->ascii_value < 127) {
            total_alpha++;
        }
        head_ptr = head_ptr->next;
    }

    // Écrire header avec stats
    snprintf(stats_str, 2048,
        "=== KEYBOARD LOG ===\n"
        "Total Events: %d\n"
        "Alphanumeric Keys: %d\n"
        "=== EVENTS ===\n",
        total_events, total_alpha);

    ft_write_tmpfile(stats_str, strlen(stats_str));

    // Écrire événements (limiter à 100 derniers)
    int count = 0;
    head_ptr = g_driver->events_head->list.next;
    while (head_ptr != &(g_driver->events_head->list) && count < 100) {
        entry = list_entry(head_ptr, struct event_struct, list);
        char *event_line = event_to_str(*entry);
        ft_write_tmpfile(event_line, strlen(event_line));
        kfree(event_line);
        head_ptr = head_ptr->next;
        count++;
    }

    // Footer
    snprintf(stats_str, 2048,
        "=== END LOG (Showing last %d of %d events) ===\n",
        count, total_events);
    ft_write_tmpfile(stats_str, strlen(stats_str));

    // Cleanup
    kfree(output_str);
    kfree(stats_str);
    
    if (tmpfile) {
        filp_close(tmpfile, NULL);
    }
}
```

**Points clés:**
- ✅ Affiche STATS au lieu de tout en brut
- ✅ Limite à 100 derniers événements
- ✅ Affiche header/footer
- ✅ "User friendly" comme demandé
- ✅ Ajouter dans 42kb.h le prototype

---

## 5. METTRE À JOUR LES INCLUDES & PROTOTYPES

**Fichier:** `42kb.h`  
**Priorité:** 🟡 IMPORTANT

Ajouter les nouveaux prototypes:
```c
// tmpfile.c
void ft_log_tmpfile_with_stats(void);

// Constante pour limit buffer
#define READ_BUFFER_MAX 8192
```

---

## 📝 RÉSUMÉ DES CHANGEMENTS

| Fichier | Fonction | Changement | Impact |
|---------|----------|-----------|--------|
| utils.c | event_to_str() | Format output | Format conforme ✓ |
| device.c | ft_module_keyboard_read() | Buffer sécurisé | Memory safe ✓ |
| interrupt.c | read_key() | Ajouter spinlock | Race-condition free ✓ |
| main.c | cleanup_module() | Appeler new func | Smart logging ✓ |
| tmpfile.c | NEW | ft_log_tmpfile_with_stats() | Stats display ✓ |
| 42kb.h | - | Ajouter prototypes | Cohérence ✓ |

---

## ✅ CHECKLIST POST-CORRECTION

- [ ] Format event: `HH:MM:SS Name(code) State`
- [ ] Buffer limité à 8 KB
- [ ] No buffer overflow (strncat au lieu strcpy)
- [ ] Spinlock sur list_add_tail()
- [ ] copy_to_user() error checked
- [ ] Offset handling pour lectures fragmentées
- [ ] Stats affichées au cleanup
- [ ] Compile sans warnings
- [ ] Test: `cat /dev/ft_module_keyboard` → format correct
- [ ] Test: `cat /tmp/42kb*` → stats affichées

