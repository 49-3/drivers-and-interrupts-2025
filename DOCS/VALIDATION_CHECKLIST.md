# ✅ CHECKLIST DE VÉRIFICATION CODE

**Utiliser cette checklist après chaque implémentation pour valider**

---

## 📋 PHASE 1 - OBLIGATOIRE

### 1. Format Output - `utils.c`

- [ ] Fonction `event_to_str()` retourne format correct
- [ ] Format: `HH:MM:SS Name(code) State`
- [ ] Exemple: `14:32:15 e(18) Pressed`
- [ ] Utilise `snprintf()` avec limite de taille
- [ ] `%02d` pour padding zéro des heures/min/sec
- [ ] Code en décimal (pas hex)
- [ ] "Pressed" vs "Released" correct
- [ ] Pas d'espaces parasites
- [ ] Test: `sprintf` fait 256 bytes max

**Vérification rapide:**
```c
// Dans event_to_str():
// ✓ snprintf(output_str, 256, "%02d:%02d:%02d %s(%d) %s\n", ...)
// ✓ event.is_pressed ? "Pressed" : "Released"
```

---

### 2. Buffer Sécurisé - `device.c`

- [ ] `#define READ_BUFFER_MAX 8192` au top du fichier
- [ ] `kmalloc(READ_BUFFER_MAX, GFP_KERNEL)` utilisé
- [ ] Vérifier retour: `if (!output_str) return -ENOMEM;`
- [ ] `strncat()` utilisé au lieu `strcpy()`
- [ ] Loop vérifie: `current_len + needed < READ_BUFFER_MAX`
- [ ] Buffer initialized: `output_str[0] = '\0';`
- [ ] Pas d'allocation sans limite
- [ ] `copy_to_user()` retour est checké
- [ ] `kfree(output_str)` appelé en toutes sorties

**Vérification rapide:**
```c
// ✓ Pas de: kmalloc(69420 * 42, ...)
// ✓ Pas de: strcpy(dest, src)
// ✓ Oui à: strncat(dest, src, max_size)
// ✓ Oui à: if (!output_str) return -ENOMEM;
```

---

### 3. Spinlock List - `interrupt.c`

- [ ] `spin_lock(&devfile_io_spinlock);` AVANT `list_add_tail()`
- [ ] `list_add_tail()` protégée
- [ ] `g_driver->total_events++;` incrémenté (bonus)
- [ ] `spin_unlock(&devfile_io_spinlock);` APRÈS
- [ ] Pas de deadlock (même spinlock partout)
- [ ] Spinlock utilisé dans `read()` aussi
- [ ] Spinlock utilisé dans `cleanup_module()` aussi

**Vérification rapide:**
```c
// ✓ Dans read_key():
spin_lock(&devfile_io_spinlock);
list_add_tail(&(event->list), &(...));
g_driver->total_events++;
spin_unlock(&devfile_io_spinlock);
```

---

### 4. Offset Handling - `device.c` read()

- [ ] Check si `*offset >= current_len` → return 0
- [ ] Calculate `remaining = current_len - *offset`
- [ ] Calculate `to_copy = min(buff_size, remaining)`
- [ ] Copier depuis `output_str + *offset`
- [ ] Update `*offset += to_copy - did_not_cpy`
- [ ] Check `copy_to_user()` retour
- [ ] Gérer lectures fragmentées (multiples `read()`)
- [ ] Buffer reste cohérent entre appels

**Vérification rapide:**
```c
// ✓ if (*offset >= current_len) { kfree(); return 0; }
// ✓ copy_to_user(buff, output_str + *offset, to_copy);
// ✓ *offset += (to_copy - did_not_cpy);
```

---

### 5. Smart Logging - `tmpfile.c`

- [ ] Fonction `ft_log_tmpfile_with_stats()` créée
- [ ] Compte total d'événements
- [ ] Compte touches alphanumériques
- [ ] Affiche header: `=== KEYBOARD LOG ===`
- [ ] Affiche stats: `Total Events: XX`
- [ ] Affiche événements (limité à 100 max)
- [ ] Affiche footer: `=== END LOG ===`
- [ ] Format user-friendly (lisible)
- [ ] Pas d'overflow mémoire

**Vérification rapide:**
```c
// ✓ Nouveau prototype dans 42kb.h:
void ft_log_tmpfile_with_stats(void);

// ✓ Dans cleanup_module():
ft_log_tmpfile_with_stats();
```

---

### 6. Prototypes & Includes - `42kb.h`

- [ ] Nouveau prototype `ft_log_tmpfile_with_stats()` ajouté
- [ ] `#define READ_BUFFER_MAX 8192` ajouté
- [ ] Pas de double déclaration
- [ ] Fichier compile sans warning

---

## 🧪 TESTS PHASE 1

### Test 1: Compilation
```bash
cd /home/n43/Téléchargements/drivers-and-interrupts/old/
make clean
make 2>&1 | tee build.log
```
- [ ] 0 erreurs (`error:`)
- [ ] 0 warnings (`warning:`)
- [ ] `42kb.ko` créé

**Si warnings/errors:**
```bash
grep -E "error:|warning:" build.log
# Corriger et recommencer
```

---

### Test 2: Chargement du module
```bash
sudo insmod 42kb.ko
dmesg | tail -10
```
- [ ] Pas de kernel panic
- [ ] Message "USB Registration OK"
- [ ] Message "MiscDev Registration OK"
- [ ] Message "IRQ Registration OK"
- [ ] Message "Module initialized"

---

### Test 3: Device existe
```bash
ls -la /dev/ft_module_keyboard
```
- [ ] Fichier existe
- [ ] Permissions correctes (crwxrwx---)
- [ ] Nombre majeur/mineur affichés

---

### Test 4: Format output
```bash
# Appuyer sur clavier: a, a, e, e
cat /dev/ft_module_keyboard | head -8
```

**Devrait afficher:**
```
14:32:15 a(2) Pressed
14:32:15 a(2) Released
14:32:16 e(18) Pressed
14:32:16 e(18) Released
14:32:16 e(18) Pressed
14:32:16 e(18) Released
```

**Checklist output:**
- [ ] Temps format `HH:MM:SS` (avec padding zéro)
- [ ] Nom de touche correct
- [ ] Code en décimal (pas hex)
- [ ] "Pressed" ou "Released"
- [ ] Ligne par événement
- [ ] Newline à la fin de chaque ligne

---

### Test 5: Stats au cleanup
```bash
sudo rmmod 42kb
dmesg | tail -20 | grep -A 5 "Total Events"
```

- [ ] Affiche "Total Events: XX"
- [ ] Affiche "Alphanumeric Keys: YY"
- [ ] Stats dans log kernel

---

### Test 6: Fichier /tmp
```bash
ls -la /tmp/42kb*
cat /tmp/42kb*
```

- [ ] Fichier créé: `/tmp/42kb<timestamp>`
- [ ] Affiche header: `=== KEYBOARD LOG ===`
- [ ] Affiche "Total Events"
- [ ] Affiche "Alphanumeric Keys"
- [ ] Affiche événements
- [ ] Affiche footer
- [ ] Pas de corruption mémoire

---

## 🔒 SÉCURITÉ CHECKLIST

- [ ] Pas d'allocation > 8 KB pour buffer
- [ ] Toutes allocations avec vérification retour
- [ ] Pas de `strcpy()` sans limite
- [ ] Pas de `sprintf()` sans snprintf()
- [ ] Spinlock sur toute modification de liste
- [ ] Pas d'accès race condition détectables
- [ ] Tous `kfree()` appelés après `kmalloc()`
- [ ] Pas de NULL pointer dereference
- [ ] `copy_to_user()` retour checké
- [ ] Offset vérifié avant accès buffer

---

## ⭐ BONUS - PRE-CHECKLIST

Si Phase 1 OK, faire Bonus:

### Bonus 1: Stats Avancées
- [ ] `compute_key_statistics()` implémentée
- [ ] Top 5 keys affichées
- [ ] Compte correct par touche
- [ ] Format lisible

### Bonus 2: Hotplug
- [ ] `handle_probe()` remplit device_count
- [ ] `handle_disconnect()` vide device_count
- [ ] Logs branchement/débanchement
- [ ] Pas de memory leak

### Bonus 3: Real Driver
- [ ] `write_to_tty()` implémentée
- [ ] TTY trouvé correctement
- [ ] Messages affichés dans terminal
- [ ] Pas de crash

---

## 🚀 FINAL VALIDATION

Avant submission, vérifier:

### Code Quality
- [ ] Code compile sans warning
- [ ] Code compile sans error
- [ ] Indentation 4 spaces ou tabs consistant
- [ ] Commentaires clairs
- [ ] Variable names explicites

### Functionality
- [ ] Obligatoire 100% conforme
- [ ] Tous les tests passent
- [ ] Pas de memory leak (valgrind OK)
- [ ] Pas de kernel panic
- [ ] Performance acceptable

### Documentation
- [ ] Commentaires dans code
- [ ] Fonction bien documentées
- [ ] `42kb.h` complet

---

## 📝 PLAN D'EXECUTION

```
1. Checklist Code Quality (5 min)
2. Checklist PHASE 1 items (5 min)
3. Compiler & test (30 min)
4. Fixer issues trouvées (30 min)
5. Checklist Sécurité (10 min)
6. OPTIONAL Bonus (1-2h)
7. Final validation (15 min)

Total: 2-4 heures
```

---

## ✅ PRÊT À SUBMIT?

Si tout ✅ en green:
1. Commit tout
2. Push sur git
3. Vérifier sur le repo
4. Ready pour defense!

**Good luck! 🎉**

