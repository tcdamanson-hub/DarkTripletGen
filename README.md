# Dark Triplet Gen — VST3 générateur de mélodies dark trap

Plugin (VST3 + AU sur Mac + application Standalone) qui génère des mélodies **dark trap** en **triolets**, en **binaire (normal)** ou en **mix des deux**, sur **57 gammes/modes** — et n'importe quel mode de n'importe quelle gamme grâce à la molette *Mode rot.*

## Fonctionnalités

- **Rythme** : `Triplets` (triolets de croches, de doubles, de noires, rolls en triolets) · `Straight` (doubles croches, croches pointées, 3-3-2, rolls en triples croches) · `Mix`
- **Gammes** : 7 modes majeurs, 7 modes du mineur harmonique, 7 du mineur mélodique, 7 du majeur harmonique, 7 du double harmonique, napolitaines, persane, énigmatique, hongroises, pentatoniques, japonaises (Hirajoshi, In Sen, Iwato, Kumoi), Pelog, blues, gamme par tons, diminuées, augmentée, Prometheus, chromatique…
- **Darkness** : favorise b2, b3, b6, triton, tonique et quinte, avec une tendance descendante
- Structure musicale **A A' A B** (question/réponse) sur 1, 2, 4 ou 8 mesures, fin résolue sur la tonique
- **Seed** : même seed = même mélodie (sauvegardée avec ton projet)
- **Export MIDI** : bouton `EXPORT .MID` ou zone `GLISSER MIDI >>` à faire glisser directement dans ta DAW
- **Sortie MIDI** en temps réel, synchronisée au tempo et à la lecture de la DAW
- **Synthé intégré** (2 saws + sub, filtre, glide) pour pré-écouter, désactivable

## Réglages

| Réglage | Rôle |
|---|---|
| Tonique / Gamme / Mode rot. | Tonalité ; *Mode rot.* fait tourner la gamme (ex. Harmonic Minor + rot. 4 = Phrygian Dominant) |
| Rythme | Triplets / Straight / Mix |
| Mesures | Longueur de la boucle |
| Octave / Etendue | Registre de la tonique et ambitus (1 à 3 octaves) |
| Densité | Peu → beaucoup de notes |
| Darkness | 0 = neutre, 1 = très sombre |
| Rolls | Quantité de rolls rapides |
| Silences | Quantité de temps vides |
| Sauts 8ve | Notes envoyées à l'octave supérieure |
| Gate | Longueur des notes (staccato → legato) |
| Seed | Variation ; le bouton **GENERER** en tire une nouvelle au hasard |
| Cutoff / Glide / Volume | Son du synthé de pré-écoute |

**PREVIEW** joue la boucle même quand ta DAW est à l'arrêt (au tempo de la DAW, 140 BPM par défaut).

## Obtenir le plugin

### Option A — sans rien installer (GitHub)
1. Crée un dépôt GitHub et mets-y le contenu de ce dossier (le dossier `.github` compris).
2. Onglet **Actions** → la compilation démarre toute seule (~10 min).
3. Télécharge l'artefact `DarkTripletGen-Windows` ou `DarkTripletGen-macOS`.

### Option B — compiler sur ton PC
Prérequis : **CMake 3.22+**, **Git**, et un compilateur C++ :
- Windows : Visual Studio 2022 (charge de travail « Développement Desktop en C++ »)
- Mac : Xcode

```bash
cd DarkTripletGen
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE est téléchargé automatiquement au premier lancement.

Le plugin se trouve dans `build/DarkTripletGen_artefacts/Release/VST3/Dark Triplet Gen.vst3`.

### Installation
- Windows : copier `Dark Triplet Gen.vst3` dans `C:\Program Files\Common Files\VST3\`
- Mac : `~/Library/Audio/Plug-Ins/VST3/` (et le `.component` dans `~/Library/Audio/Plug-Ins/Components/` pour Logic)

Puis relancer un scan des plugins dans la DAW. Il apparaît comme **instrument**.

## Utilisation dans la DAW

- **Le plus simple (toutes DAW)** : génère jusqu'à ce que ça te plaise, puis fais glisser `GLISSER MIDI >>` dans le piano roll / une piste, et mets ton propre son dessus.
- **FL Studio** : charge-le dans le Channel Rack. Pour piloter un autre synthé en direct : dans les réglages du plugin (roue dentée), règle un *MIDI output port*, et mets le même *input port* sur le synthé cible.
- **Ableton** : piste MIDI avec Dark Triplet Gen ; sur une autre piste MIDI, « MIDI From » = la piste du plugin.
- **Reaper / Bitwig / Studio One** : le MIDI sortant peut être routé directement vers une autre piste.

## Fichiers

- `Source/Scales.h` — toutes les gammes (facile d'en ajouter)
- `Source/MelodyGenerator.cpp` — l'algorithme (cellules rythmiques, poids "dark", structure A A' A B)
- `Source/DarkSynth.h` — synthé de pré-écoute
- `Source/PluginProcessor.cpp` — paramètres, synchro DAW, sortie et export MIDI
- `Source/PluginEditor.cpp` — interface et piano roll
