# 🗺️ ROADMAP COMPLÈTE - De Zéro à Hero

**Vous avez tous les outils. Voici le chemin exact.**

---

## 📚 DOCUMENTS DISPONIBLES

| Document | Contenu | Durée |
|----------|---------|-------|
| **📖 PDF** | Spécification complète de l'exercice | 20 min |
| **📋 ANALYSE_EXERCICE.md** | Comparaison spec vs code | 15 min |
| **🔧 CODE_EXACT_MODIFICATIONS.md** | Code exact à copier/coller | 60 min |
| **✅ VALIDATION_CHECKLIST.md** | Tests & validation | 30 min |
| **🎁 BONUS_GUIDE.md** | Bonus implementé | 120 min |

---

## 🎯 PHASE 1: CORRECTIONS OBLIGATOIRES (60-90 min)

### Step 1: Préparation (5 min)
```bash
cd /home/n43/Téléchargements/drivers-and-interrupts/old
git status  # Vérifier diffs
```

### Step 2: Modifications (50 min)
**Suivre CODE_EXACT_MODIFICATIONS.md point par point:**

1. ✏️ Modifier `42kb.h` - Ajouter prototypes (2 min)
2. ✏️ Modifier `utils.c` - event_to_str() (5 min)
3. ✏️ Modifier `interrupt.c` - Spinlock (5 min)
4. ✏️ Modifier `device.c` - Buffer safe (15 min)
5. ✏️ Modifier `tmpfile.c` - Nouvelle fonction (20 min)
6. ✏️ Modifier `main.c` - Appeler nouvelle fonction (2 min)

**APRÈS CHAQUE MODIFICATION:**
```bash
make clean && make
# ✅ Doit compiler sans errors
```

### Step 3: Test (30 min)
**Suivre VALIDATION_CHECKLIST.md:**

```bash
# 1. Insert module
sudo insmod 42kb.ko

# 2. Générer événements clavier
xdotool key a b c

# 3. Lire output
cat /dev/ft_module_keyboard

# 4. Vérifier format
cat /tmp/42kb*

# 5. Remove module
sudo rmmod 42kb
```

### Step 4: Validation (5 min)
✅ Output format exact: `HH:MM:SS Name(code) State`
✅ Timestamps corrects
✅ Pas de crash kernel
✅ Fichier /tmp généré avec stats

---

## 🎁 PHASE 2: BONUS (120+ min)

**SEULEMENT APRÈS phase 1 réussie!**

### Bonus 1: Macro de log améliorée (30 min)
```c
#define FT_LOG_LEVEL_DEBUG    0
#define FT_LOG_LEVEL_INFO     1
#define FT_LOG_LEVEL_WARNING  2
#define FT_LOG_LEVEL_ERROR    3

int ft_current_log_level = FT_LOG_LEVEL_INFO;

void ft_log_internal(int level, const char *fmt, ...) {
    if (level >= ft_current_log_level) {
        const char *level_str[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        printk(KERN_INFO "%s ", level_str[level]);
        va_list args;
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
    }
}

#define ft_log(fmt, ...) ft_log_internal(FT_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define ft_debug(fmt, ...) ft_log_internal(FT_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define ft_warn(fmt, ...) ft_log_internal(FT_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define ft_error(fmt, ...) ft_log_internal(FT_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
```

### Bonus 2: Statistiques détaillées (40 min)
```c
typedef struct {
    unsigned long total_keys;
    unsigned long shift_keys;
    unsigned long ctrl_keys;
    unsigned long alt_keys;
    unsigned long special_keys;
    unsigned long function_keys;
    int most_pressed_key;
    int most_pressed_count;
} stats_t;

void ft_collect_stats(stats_t *stats) {
    struct list_head *head_ptr;
    struct event_struct *entry;
    
    spin_lock(&devfile_io_spinlock);
    head_ptr = g_driver->events_head->list.next;
    while (head_ptr != &(g_driver->events_head->list)) {
        entry = list_entry(head_ptr, struct event_struct, list);
        
        if (entry->is_pressed) {
            stats->total_keys++;
            // Analyser scancode...
        }
        head_ptr = head_ptr->next;
    }
    spin_unlock(&devfile_io_spinlock);
}
```

### Bonus 3: Filtering & recherche (50 min)
```c
int ft_module_keyboard_write(struct file *file, const char *buff, 
                              size_t count, loff_t *offset)
{
    char filter[256];
    
    if (copy_from_user(filter, buff, min(count, 255UL))) {
        return -EFAULT;
    }
    filter[count] = '\0';
    
    // Appliquer le filtre
    if (strncmp(filter, "stats", 5) == 0) {
        // Afficher stats
    } else if (strncmp(filter, "clear", 5) == 0) {
        // Nettoyer liste
    } else if (strncmp(filter, "filter:", 7) == 0) {
        // Filtrer par type
    }
    
    return count;
}
```

---

## 🏁 CHECKLIST FINALE

### Phase 1 Obligatoire
- [ ] Code compile sans errors
- [ ] Pas de warnings importants
- [ ] Format output correct: `HH:MM:SS Name(code) State`
- [ ] Timestamps exacts
- [ ] Fichier /tmp généré avec stats
- [ ] Module charge/décharge sans crash
- [ ] Pas de race conditions détectées

### Phase 2 Bonus (Optional)
- [ ] Logging amélioré avec niveaux
- [ ] Stats détaillées affichées
- [ ] Filtering & recherche fonctionnelle
- [ ] Bonus tests réussissent

---

## 🐛 DEBUGGING

### Si compile échoue:
```bash
# Vérifier les erreurs
make 2>&1 | head -20

# Vérifier warnings
make 2>&1 | grep -i warning

# Nettoyer et retry
make clean && make
```

### Si module ne charge pas:
```bash
# Vérifier dmesg
sudo dmesg | tail -20

# Vérifier module
lsmod | grep 42kb
```

### Si pas d'événements captés:
```bash
# Vérifier IRQ
cat /proc/interrupts | grep i8042

# Vérifier device
ls -la /dev/ft_module_keyboard

# Test simple
echo "test" > /dev/ft_module_keyboard
```

### Si output incorrect:
```bash
# Vérifier format exact
cat /dev/ft_module_keyboard | head -1

# Doit être: HH:MM:SS Name(code) State
# Pas: [HH:MM:SS] Name(0xHEX) State
```

---

## 📞 AIDE RAPIDE

**Q: Où sont les fichiers source?**
A: `/home/n43/Téléchargements/drivers-and-interrupts/old/`

**Q: Comment compiler?**
A: `make clean && make`

**Q: Comment tester?**
A: Voir VALIDATION_CHECKLIST.md

**Q: Quelles modifications faire?**
A: Voir CODE_EXACT_MODIFICATIONS.md

**Q: Comment faire les bonus?**
A: Voir BONUS_GUIDE.md

---

## ⏱️ TIMING TOTAL

| Phase | Tâche | Durée | Cumul |
|-------|-------|-------|-------|
| 1 | Modifications | 60 min | 60 min |
| 1 | Test & debug | 30 min | 90 min |
| 2 | Bonus 1 (opt) | 30 min | 120 min |
| 2 | Bonus 2 (opt) | 40 min | 160 min |
| 2 | Bonus 3 (opt) | 50 min | 210 min |

**Obligatoire: 90 minutes**
**Avec bonus: 3.5 heures**

---

## 🚀 COMMENCER MAINTENANT

1. **Lire** CODE_EXACT_MODIFICATIONS.md (5 min)
2. **Appliquer** les 6 modifications (50 min)
3. **Compiler** et fixer erreurs (15 min)
4. **Tester** avec VALIDATION_CHECKLIST.md (30 min)
5. **Ajouter** bonus si temps (120+ min)

**ALLEZ-Y! 💪**

