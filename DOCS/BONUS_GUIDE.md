# ⭐ GUIDE BONUS - Implémentation supplémentaire

**Important:** Les bonus ne seront évalués QUE si la partie obligatoire est PARFAITE.

---

## 🎁 BONUS 1: Log avec Stats Avancées

**Difficulté:** ⭐ Facile  
**Temps:** 30-45 min  
**Points:** +2-5%

### Objectif
Améliorer `ft_log_tmpfile_with_stats()` pour afficher:
- Top 5 touches les plus pressées
- Durée totale de pression par touche
- Entropie des entrées
- Timeline (temps premier/dernier événement)

### Implémentation

**Ajouter struct pour stats:**
```c
typedef struct {
    char key;
    int count;
    time64_t first_time;
    time64_t last_time;
} key_stat_t;
```

**Nouvelle fonction dans `utils.c`:**
```c
void compute_key_statistics(key_stat_t *stats_array, int *count) {
    struct list_head *head_ptr;
    struct event_struct *entry;
    int i, found;
    
    *count = 0;
    head_ptr = g_driver->events_head->list.next;
    
    while (head_ptr != &(g_driver->events_head->list)) {
        entry = list_entry(head_ptr, struct event_struct, list);
        
        // Si c'est une touche alphanumeric pressée
        if (entry->is_pressed && entry->ascii_value > 32 && entry->ascii_value < 127) {
            found = 0;
            
            // Chercher si déjà dans array
            for (i = 0; i < *count; i++) {
                if (stats_array[i].key == entry->ascii_value) {
                    stats_array[i].count++;
                    stats_array[i].last_time = entry->time;
                    found = 1;
                    break;
                }
            }
            
            // Ajouter nouvelle touche si pas trouvée
            if (!found && *count < 256) {
                stats_array[*count].key = entry->ascii_value;
                stats_array[*count].count = 1;
                stats_array[*count].first_time = entry->time;
                stats_array[*count].last_time = entry->time;
                (*count)++;
            }
        }
        
        head_ptr = head_ptr->next;
    }
    
    // Trier par count (bubble sort)
    for (i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (stats_array[j].count < stats_array[j + 1].count) {
                key_stat_t temp = stats_array[j];
                stats_array[j] = stats_array[j + 1];
                stats_array[j + 1] = temp;
            }
        }
    }
}
```

**Améliorer `ft_log_tmpfile_with_stats()`:**
```c
void ft_log_tmpfile_with_stats(void) {
    // ... code précédent ...
    
    key_stat_t stats[256];
    int stats_count = 0;
    struct tm first_tm, last_tm;
    
    // Calculer stats
    compute_key_statistics(stats, &stats_count);
    
    // Écrire stats avancées
    snprintf(stats_str, 2048, "=== TOP KEYS ===\n");
    ft_write_tmpfile(stats_str, strlen(stats_str));
    
    for (int i = 0; i < (stats_count < 5 ? stats_count : 5); i++) {
        time64_t duration = stats[i].last_time - stats[i].first_time;
        snprintf(stats_str, 2048, 
            "  %d. '%c' - %d times, Duration: %lds\n",
            i + 1, stats[i].key, stats[i].count, duration);
        ft_write_tmpfile(stats_str, strlen(stats_str));
    }
}
```

---

## 🎁 BONUS 2: Support Hotplug (Clavier Branché/Débranché)

**Difficulté:** ⭐⭐ Moyen  
**Temps:** 1-1.5h  
**Points:** +5-10%

### Objectif
Gérer `handle_probe()` et `handle_disconnect()` correctement

### Implémentation

**Fichier:** `usb.c`

```c
// Structure pour tracker chaque device
typedef struct {
    struct usb_device *udev;
    struct usb_interface *interface;
} keyboard_device_t;

static keyboard_device_t *keyboard_devices[10];
static int device_count = 0;

int handle_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(intf);
    
    ft_log("Keyboard plugged in!");
    printk(KERN_INFO "[42-KB] Device: Vendor=0x%04x Product=0x%04x\n",
        udev->descriptor.idVendor, udev->descriptor.idProduct);
    
    if (device_count < 10) {
        keyboard_devices[device_count] = kmalloc(sizeof(keyboard_device_t), GFP_KERNEL);
        if (!keyboard_devices[device_count])
            return -ENOMEM;
        
        keyboard_devices[device_count]->udev = udev;
        keyboard_devices[device_count]->interface = intf;
        device_count++;
    }
    
    return 0;
}

void handle_disconnect(struct usb_interface *intf) {
    int i;
    
    ft_log("Keyboard unplugged!");
    
    // Trouver et supprimer de la liste
    for (i = 0; i < device_count; i++) {
        if (keyboard_devices[i]->interface == intf) {
            kfree(keyboard_devices[i]);
            // Décaler les suivants
            for (int j = i; j < device_count - 1; j++) {
                keyboard_devices[j] = keyboard_devices[j + 1];
            }
            device_count--;
            break;
        }
    }
}
```

---

## 🎁 BONUS 3: Intercepter le Clavier Original et Écrire dans TTY

**Difficulté:** ⭐⭐⭐ Très difficile  
**Temps:** 2-3h  
**Points:** +10-15%

### Contexte
C'est un vrai driver qui:
1. Désactive le driver original
2. Intercepte les interruptions clavier
3. Écrit les événements dans le TTY de l'utilisateur
4. Devient un driver "transparent"

### Implémentation (Complexe)

**Dans utils.c - Écrire dans TTY:**
```c
void write_to_tty(const char *message) {
    struct tty_struct *tty;
    
    if (!g_driver || !g_driver->task)
        return;
    
    // Obtenir le TTY associé à la task
    tty = get_current_tty();
    
    if (tty != NULL) {
        tty->ops->write(tty, message, strlen(message));
        tty_wakeup(tty);
    }
}
```

**Améliorer le handler d'interruption:**
```c
irqreturn_t handler(int irq, void *dev_id) {
    unsigned char scancode = inb(KB_PORT);
    event_struct *event;
    char tty_output[128];
    
    // Lire la touche
    schedule_work(&(q_data->worker));
    
    // Optionnel: écrire aussi dans TTY
    // sprintf(tty_output, "[%02d:%02d:%02d] Key: 0x%X\n", ...);
    // write_to_tty(tty_output);
    
    return IRQ_HANDLED;
}
```

**Problèmes et Solutions:**

| Problème | Solution |
|----------|----------|
| Driver original actif | Blacklist ou rmmod avant |
| TTY dynamique | Trouver task depuis /proc |
| Synchronisation TTY | Utiliser workqueue |
| Clavier multiple | Gérer plusieurs interfaces |

### ⚠️ Limitations
- Très dépendant de la version du kernel
- Peut causer kernel panic mal fait
- Require désactivation driver original

---

## 📊 TABLEAU RÉCAP BONUS

| Bonus | Difficulté | Temps | Points | Status |
|-------|-----------|-------|--------|--------|
| Stats avancées | ⭐ | 45min | +2-5% | Faisable |
| Hotplug | ⭐⭐ | 1.5h | +5-10% | Recommandé |
| Real TTY Driver | ⭐⭐⭐ | 3h+ | +10-15% | Risqué |

---

## 🎯 ORDRE RECOMMANDÉ

1. **D'abord:** Parfaire la partie obligatoire (100%)
2. **Bonus 1:** Stats avancées (Easy win +2-5%)
3. **Bonus 2:** Hotplug (Moyen effort, bon retour)
4. **Bonus 3:** Real driver (Si reste du temps ET confiance)

---

## ✅ BONUS 1 - CHECKLIST

- [ ] Fonction `compute_key_statistics()` implémentée
- [ ] Top 5 keys affichées correctement
- [ ] Duration calculée
- [ ] Format lisible
- [ ] Pas de memory leak
- [ ] Test: Appuyer sur 'e', 'a', etc et vérifier top 5

---

## ✅ BONUS 2 - CHECKLIST

- [ ] `handle_probe()` remplit device_count
- [ ] `handle_disconnect()` vide device_count
- [ ] Logs affichés lors plug/unplug
- [ ] Max 10 devices supportés
- [ ] Pas de memory leak
- [ ] Test: Brancher/débrancher clavier et vérifier logs

---

## ✅ BONUS 3 - CHECKLIST

- [ ] `write_to_tty()` implémentée
- [ ] TTY trouvé correctement
- [ ] Messages affichés dans terminal
- [ ] Pas de crash
- [ ] Synchronisation correcte
- [ ] Test: Vérifier lettres dans TTY

---

## 🚨 AVERTISSEMENTS

> ⚠️ Bonus 3 (Real Driver) peut causer kernel panic
> 
> Tester en VM safe, avec snapshots avant test !

> ⚠️ Ne pas faire Bonus 3 si Bonus 1/2 en danger
> 
> Les bonus sont EXTRA, pas critique pour la note

> ⚠️ Une seule partie obligatoire doit être PARFAITE
> 
> Sinon ZÉRO bonus même si ça compile

---

## 📈 SCORE ESTIMÉ

**Sans bonus:** 60-75%  
**Avec Bonus 1:** 75-80%  
**Avec Bonus 1+2:** 85-92%  
**Avec Bonus 1+2+3:** 95-100%

(Si tout fonctionne parfaitement)

