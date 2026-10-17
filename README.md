# Textural (Studio Wiga) — VST3 granulaire pour Windows 11

Moteur de synthèse granulaire inspiré de la pédale MXR Textures M310 : trois modes (Delay, Auto Freeze, Layers), grains inversés, pitch ±2 octaves, régénération delay/granulaire, randomisations, reverb, filtre LP/HP 4 pôles, vibrato, tap tempo, trails, sorties Mono / Wet-Dry / Stéréo, 8 presets d'usine et contrôle MIDI complet.

## Compiler avec GitHub

1. Pousse **tous les fichiers à la racine** du dépôt (dont `faceplate.png` et le dossier `.github/workflows`).
2. GitHub Actions compile automatiquement (onglet **Actions** → dernier run → *Artifacts*) :
   - `Textural-Setup-Windows` : installeur `Textural-Setup.exe` (copie le plugin dans `C:\Program Files\Common Files\VST3`) ;
   - `Textural-VST3-Windows` : le dossier `Textural.vst3` à copier soi-même dans `C:\Program Files\Common Files\VST3`.
3. Relance un scan des plugins dans ton DAW : le plugin apparaît sous **Textural**, fabricant **Studio Wiga**.

## Les trois modes (bouton MODE, à droite de REVERSE)

La LED du bouton prend la couleur du symbole du mode et clignote quand le son est gelé. Le nom du mode s'affiche dans la ligne d'état.

- **Delay (vert)** : l'entrée est enregistrée en continu et rejouée en grains, TIME derrière la tête d'écriture. REGEN réinjecte la sortie delay et/ou la sortie des grains (Regen Mix).
- **Auto Freeze (orange)** : à chaque note ou accord détecté, la pédale enregistre pendant TIME puis gèle le buffer. Les grains y puisent sans fin, ce qui donne une nappe qui évolue. Une nouvelle note remplace la précédente.
- **Layers (rouge)** : jusqu'à 4 notes ou accords dans 4 buffers indépendants (le 5e remplace le plus ancien). TIME règle le fondu d'entrée (attaque) et REGEN le fondu de sortie (déclin), comme une voix de synthé. C'est le mode synthé polyphonique : monte REGEN pour tenir les notes.

## Utilisation

| Action | Effet |
|---|---|
| Glisser un bouton (haut/bas) | Réglage ; **Maj** = réglage fin ; double-clic = valeur par défaut ; molette OK |
| Clic REVERSE | Grains inversés on/off (LED bleue) |
| Clic MODE | Delay (vert) → Auto Freeze (orange) → Layers (rouge) ; la LED prend la couleur du symbole et clignote quand le son est gelé |
| Clic footswitch | Effet on/off (LED bleue) |
| Maintien footswitch | Freeze momentané (Delay/Layers) ; en Auto Freeze, le maintien *dé-gèle*. Si *Footswitch Hold Mode* = Preset Scroll : maintien = entrer/sortir du défilement, clic = preset suivant |
| Clic bouton PRESETS | Live → 1 … 8 → Live (LED verte = 1-4, rouge = 5-8, clignote si modifié) |
| Maintien PRESETS 2 s | Copie des réglages ; clics = choix de l'emplacement ; maintien 2 s = sauvegarde |
| Clic droit | Taille (Default 493 px, 60 %, 75 %, 100 %), sauvegarde/réinitialisation des presets |

### Paramètres secondaires (comme maintenir les boutons de la pédale)

- **Ctrl + glisser** (ou appui long sur REVERSE pour verrouiller) : Time Random, Size Random, Overlap Random, Regen Mix (Delay ↔ Granular), Pitch Random, Reverse Random.
- **Alt + glisser** (ou appui long sur MODE) : Reverb Mix, Filter Cutoff (LP < midi < HP), Vibrato Rate, Reverb Decay, Quantize Pitch, Vibrato Depth.
- **Ctrl + Alt + glisser** (ou clic sur la LED bleue) : Preset Scroll Mute, Trails, Output Mode, Footswitch Hold Mode, Output Gain.
- **Tap tempo** : Ctrl + clic sur MODE (ou REVERSE verrouillé puis clic sur MODE), ou CC83.

La ligne d'état sous « Textural » n'affiche que le réglage du potard en cours de rotation, dans la couleur du potard.

## MIDI

Program Change 1-8 = emplacements (2 = Live, 0 = Live aussi), 9 = Slow Attack Soundtrack. Dans la liste des presets du DAW : 1 Sea Synth … 8 Stardust Cloud, puis Slow Attack Soundtrack. Tous les paramètres sont aussi automatisables dans le DAW.

| CC | Paramètre | CC | Paramètre |
|---|---|---|---|
| 14 | Time | 26 | Reverb Mix |
| 15 | Size | 27 | Reverb Decay |
| 16 | Overlap | 28 | Filter Cutoff |
| 17 | Regen | 29 | Quantize Pitch |
| 18 | Pitch | 30 | Vibrato Rate |
| 19 | Mix | 31 | Vibrato Depth |
| 20 | Time Random | 64 | Freeze (maintien) |
| 21 | Size Random | 80 | Reverse |
| 22 | Overlap Random | 81 | Mode |
| 23 | Regen Mix | 82 | Effet on (≥ 64) / off |
| 24 | Pitch Random | 83 | Tap tempo |
| 25 | Reverse Random | 84 | Preset suivant |
| 85 | Output Gain | 86 | Output Mode |
| 87 | Trails | 88 | Footswitch Hold Mode |
| 89 | Preset Scroll Mute | | |

## Presets d'usine

À l'ouverture, la pédale charge le preset 1 (Sea Synth). Le bouton PRESETS fait défiler 1 → 2 → … → 8 → 1.

Verts :
1. Sea Synth : synthé polyphonique ondulant.
2. **Live** : les réglages courants des potards, non sauvegardable.
3. Crystal Cave : boucles d'octaves régénérées.
4. Melodic Kaleidoscope : mélodies gelées, à l'endroit, à l'envers, à d'autres octaves.

Rouges :
5. Obligatory Shimmer : Regen Mix 83/17 delay/granulaire.
6. Reverse Reverb : écho inversé, chaque tranche d'environ 0,9 s de guitare est rejouée à l'envers puis passe dans la reverb.
7. Mellow Cello : nappe une octave plus bas avec vibrato, plus présente après la note.
8. Stardust Cloud : synthé granulaire en Layers avec une pluie de particules.

**Slow Attack Soundtrack** n'occupe aucun emplacement de la pédale. On le charge depuis la liste des presets du DAW (programme 9 ou Program Change 9) ou par le menu clic droit.

## Moteur audio

- **Grains** : enveloppe en cosinus qui s'adapte à OVERLAP (Hann quand les grains se chevauchent, fondu de 20 % au minimum), si bien que deux grains successifs se recouvrent toujours.
- **Alignement de phase (type WSOLA)** : quand les grains sont peu nombreux, chaque nouveau grain est décalé de ±12 ms pour être en phase avec ceux qui jouent déjà, et le niveau est normalisé par la somme des fenêtres. Un son tenu garde ainsi un niveau constant, sans creux ni décrochage, y compris en reverse.
- **Reverse** : les grains inversés ne lisent que de l'audio déjà enregistré et ne croisent jamais la tête d'écriture, donc aucun clic.
- **Reverse** : avec TIME au minimum et OVERLAP bas, c'est un écho inversé classique : la dernière tranche jouée (longueur = SIZE) est rejouée à l'envers, avec un court fondu entre les tranches.
- **Diffuseur** : un réseau de filtres passe-tout, sur le principe de Mutable Instruments Clouds, fond les grains en nappe quand OVERLAP est élevé.
- **Reverb** : plate de Dattorro modulée, de la petite pièce jusqu'au maintien infini.
- **Stéréo** : panoramique et léger décalage gauche/droite par grain.
- **Mode Layers** : chaque voix a une légère dérive de hauteur, façon synthé analogique.

## Fonctions cachées (sans boutons supplémentaires)

- **MIX au-delà de 75 %** : ajoute progressivement un nuage de particules granulaires (grains de 10 à 45 ms, panoramiques aléatoires, à l'octave ou à la douzième selon Pitch Random, inversés selon Reverse).
- **Pitch Random** : le premier tiers donne un désaccord « unisson » façon synthé (jusqu'à ±30 cents). Au-delà, les grains sautent d'intervalle harmonique.
- **Time Random au-delà de 45 %** : effet glitch, certains grains rejouent la tranche précédente (stutter).
- **Delay** : en sortie Stéréo, les répétitions font du ping-pong. Le filtre colore les répétitions : sous midi, écho analogique sombre ; au-dessus, son lo-fi aminci. Vibrato Depth ajoute un wow de bande sur les répétitions.
- **Reverb** : queue modulée et diffuse. Reverb Decay va de 0,4 s à 60 s puis passe en infini en fin de course. Le filtre éclaircit ou assombrit la queue.
- **Filtre** : la résonance augmente quand on s'éloigne de midi, ce qui donne des balayages façon synthé.
- **Mode Layers** : saturation douce de type synthé analogique.

Les presets utilisateur sont enregistrés dans le projet du DAW et dans `%APPDATA%\Studio Wiga\Textural\UserPresets.xml`.
