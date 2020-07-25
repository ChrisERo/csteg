import os
import unittest

IMG_COPIES = 'imgCoppies'
ORIG_IMGS_DIR = 'imgs'
ORIG_MSSG_SOURCE = 'mssg.txt'
EXTRACTED_MSSG_FILE = 'extracted_messages.txt'

class TestCsteg(unittest.TestCase):
    def assertMessageMatch(self):
        with open(ORIG_MSSG_SOURCE, 'r') as orig:
            original_message = orig.read()
        with open(EXTRACTED_MSSG_FILE, 'r') as extr:
            extracted_message = extr.read()
        
        return self.assertEqual(original_message, extracted_message)

    def test_writing_and_reading(self):
        if os.path.isdir(IMG_COPIES):
            result = os.system('sudo rm -r ' + IMG_COPIES)
            self.assertEqual(result, 0)
        os.mkdir(IMG_COPIES)

        imgs = os.listdir(ORIG_IMGS_DIR)
        for i in range(len(imgs)):
            with self.subTest(i=i):
                # Copy original image into imgCoppies
                img = imgs[i]
                result = os.system('cp {}/{} {}/'.format(ORIG_IMGS_DIR, img, IMG_COPIES))
                self.assertEqual(result, 0)

                # Write Message into img and read from it right after
                result = os.system('csteg.bin -w {}/{} {}'.format(IMG_COPIES, 
                    img, ORIG_MSSG_SOURCE))
                self.assertEqual(result, 0)
                result = os.system('csteg.bin -r {}/{}'.format(IMG_COPIES, img))
                self.assertEqual(result, 0)

                # Make sure original and read messages match
                self.assertMessageMatch()

if __name__ == '__main__':
    unittest.main()

