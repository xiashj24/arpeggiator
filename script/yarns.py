### from Mutable Instruments Yarns
### also refer to DivSkip: https://makingsoundmachines.com/divskip/manual/#mode-5-euclidean-classic

"""----------------------------------------------------------------------------
Arpeggiator patterns
----------------------------------------------------------------------------"""

def XoxTo16BitInt(pattern):
  uint16 = 0
  i = 0
  for char in pattern:
    if char == 'o':
      uint16 += (2 ** i)
      i += 1
    elif char == '-':
      i += 1
  assert i == 16
  return uint16


def ConvertPatterns(patterns):
  return [XoxTo16BitInt(pattern) for pattern in patterns]


lookup_tables.append(
  ('arpeggiator_patterns', ConvertPatterns([
      'o-o- o-o- o-o- o-o-',
      'o-o- oooo o-o- oooo',
      'o-o- oo-o o-o- oo-o',
      'o-o- o-oo o-o- o-oo',
      'o-o- o-o- oo-o -o-o',
      'o-o- o-o- o--o o-o-',
      'o-o- o--o o-o- o--o',
      
      'o--o ---- o--o ----',
      'o--o --o- -o-- o--o',
      'o--o --o- -o-- o-o-',
      'o--o --o- o--o --o-',
      'o--o o--- o-o- o-oo',
      
      'oo-o -oo- oo-o -oo-',
      'oo-o o-o- oo-o o-o-',
      
      'ooo- ooo- ooo- ooo-',
      'ooo- oo-o o-oo -oo-',
      'ooo- o-o- ooo- o-o-',
      
      'oooo -oo- oooo -oo-',
      'oooo o-oo -oo- ooo-',
      
      'o--- o--- o--o -o-o',
      'o--- --oo oooo -oo-',
      'o--- ---- o--- o-oo'])))
      

      
"""----------------------------------------------------------------------------
Euclidean patterns
----------------------------------------------------------------------------"""

def Flatten(l):
  if hasattr(l, 'pop'):
    for item in l:
      for j in Flatten(item):
        yield j
  else:
    yield l


def EuclideanPattern(k, n):
  pattern = [[1]] * k + [[0]] * (n - k)
  while k:
    cut = min(k, len(pattern) - k)
    k, pattern = cut, [pattern[i] + pattern[k + i] for i in xrange(cut)] + \
      pattern[cut:k] + pattern[k + cut:]
  return pattern


table = []
for num_steps in xrange(1, 33):
  for num_notes in xrange(32):
    num_notes = min(num_notes, num_steps)
    bitmask = 0
    for i, bit in enumerate(Flatten(EuclideanPattern(num_notes, num_steps))):
      if bit:
        bitmask |= (1 << i)
    table.append(bitmask)

lookup_tables_32 += [('euclidean', table)]