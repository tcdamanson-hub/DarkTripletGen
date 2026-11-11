# Sons externes livres dans le plugin : noms d'origine -> noms Ranti, et fichiers nettoyes (aucune metadonnee d'origine).
# Usage : python ranti_pack.py <dossier WAV du pack OB> <dossier de sortie "Ranti Wave"> <ranti_wave_map.json>
# Garde : categorie (Bass, Keys...), note MIDI, stereo L/R, boucle (smpl). Enleve : nom du son, marque, tags, chunks inutiles.
import sys, os, json, struct, re

def clean_wav(src, dst):
    d = open(src, 'rb').read()
    assert d[:4] == b'RIFF' and d[8:12] == b'WAVE', src
    keep = []; i = 12
    while i + 8 <= len(d):
        cid = d[i:i+4]; n = struct.unpack('<I', d[i+4:i+8])[0]; body = d[i+8:i+8+n]
        if cid in (b'fmt ', b'data', b'smpl'):
            if cid == b'smpl' and len(body) >= 36:   # boucles seulement : fabricant / produit remis a zero
                body = b'\0' * 8 + body[8:]
            keep.append(cid + struct.pack('<I', len(body)) + body + (b'\0' if n % 2 else b''))
        i += 8 + n + (n % 2)
    out = b'WAVE' + b''.join(keep)
    open(dst, 'wb').write(b'RIFF' + struct.pack('<I', len(out)) + out)

def main(src, out, mapfile):
    mp = json.load(open(mapfile, encoding='utf-8'))
    n = 0
    for cat in sorted(os.listdir(src)):
        cd = os.path.join(src, cat)
        if not os.path.isdir(cd): continue
        for patch in sorted(os.listdir(cd)):
            pd = os.path.join(cd, patch)
            if not os.path.isdir(pd): continue
            new = mp[cat + '/' + patch]
            od = os.path.join(out, cat, new); os.makedirs(od, exist_ok=True)
            used = {}
            for f in sorted(os.listdir(pd)):
                b, ext = os.path.splitext(f)
                if ext.lower() != '.wav' or re.search(r'_000[123]$', b): continue
                root = b.split(' ')[0]
                if not root.isdigit(): continue
                side = ' L' if ' L ' in b + ' ' else ' R' if ' R ' in b + ' ' else ''
                k = (root, side); used[k] = used.get(k, 0) + 1
                clean_wav(os.path.join(pd, f), os.path.join(od, '%s Ranti%s %d.wav' % (root, side, used[k])))
                n += 1
    print('sons Ranti Wave :', n, 'fichiers')

if __name__ == '__main__':
    main(*sys.argv[1:4])
