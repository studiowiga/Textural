# Textural (Studio Wiga) — VST3 granulaire pour Windows 11

Moteur de synthèse granulaire inspiré de la pédale MXR Textures M310 : trois modes (Delay, Auto Freeze, Layers), grains inversés, pitch ±2 octaves, régénération delay/granulaire, randomisations, reverb, filtre LP/HP 4 pôles, vibrato, tap tempo, trails, sorties Mono / Wet-Dry / Stéréo, 8 presets d'usine et contrôle MIDI complet.

## Compiler avec GitHub

1. Pousse **tous les fichiers à la racine** du dépôt (dont `faceplate.png` et le dossier `.github/workflows`).
2. GitHub Actions compile automatiquement (onglet **Actions** → dernier run → *Artifacts*) :
   - `Textural-Setup-Windows` : installeur `Textural-Setup.exe` (copie le plugin dans `C:\Program Files\Common Files\VST3`) ;
   - `Textural-VST3-Windows` : le dossier `Textural.vst3` à copier soi-même dans `C:\Program Files\Common Files\VST3`.
3. Relance un scan des plugins dans ton DAW : le plugin apparaît sous **Textural**, fabricant **Studio Wiga**.

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

La ligne d'état sous « Textural » affiche les 6 valeurs MIDI (0-127) de la couche active, puis la valeur réelle du dernier réglage touché et son CC.

## MIDI

Program Change 0 = Live, 1-8 = presets. Tous les paramètres sont aussi automatisables dans le DAW.

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

Verts : 1 Glass Grotto (boucles d'octave régénérées) · 2 Kaleidoscope (Auto Freeze, inversé, octaves aléatoires) · 3 Tidal Synth (Layers polyphonique avec vibrato) · 4 Halo Shimmer (shimmer granulaire).
Rouges : 5 Glitch Stutter (micro-grains inversés) · 6 Slow Horizon (nappe à attaque lente) · 7 Sub Ripple (cascade sub-octave) · 8 Velvet Cello (nappe sub-octave avec vibrato).

Les presets utilisateur sont enregistrés dans le projet du DAW et dans `%APPDATA%\Studio Wiga\Textural\UserPresets.xml`.
