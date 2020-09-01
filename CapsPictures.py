import pygame
import numpy as np
from PIL import Image

def get_centers(img_width = 0, img_height = 0, radius = 0):
    centers = []

    delta_vertical   = np.ceil(np.sqrt(2) * radius)
    delta_horizontal = radius * 2

    current_y = radius
    indent = False

    while (current_y + radius < img_height):
        current_x = radius
        if indent:
            current_x += radius
        while (current_x + radius < img_width):
            centers.append([current_x, current_y])
            current_x += delta_horizontal
        current_y += delta_vertical
        indent = not indent

    return centers

def get_grey_value(image = None, center_x = 0, center_y = 0, radius = 0):

    start_x = int(center_x - radius)
    start_y = int(center_y - radius)
    end_x   = int(center_x + radius)
    end_y   = int(center_y + radius)

    matrix = image[start_x:end_x, start_y:end_y]
    return np.average(matrix)

def get_images_indexes(image_data = None, caps_data = [], centers = [], radius = 0):
    
    indexes = []

    for c in centers:
        gray_value = get_grey_value(image = image_data, center_x = c[0], center_y = c[1], radius = radius)
        indexes.append(int(gray_value / 255 * 121))
    
    min_ind = np.min(indexes)
    max_ind = np.max(indexes)

    for i in range(len(indexes)):
        indexes[i] = int((indexes[i] - min_ind) / max_ind * 120)

    return indexes

def main():
    pygame.init()
    pygame.font.init()

    caps_images = []
    caps_pngs = []

    # Change the parameters HERE
    caps_diameter = 20
    n_of_caps_per_row = 46

    for i in range(121):
        caps_pngs.append(pygame.image.load('Caps/Cap ' + str(i + 1) + '.png'))
        caps_pngs[i] = pygame.transform.scale(caps_pngs[i], (caps_diameter, caps_diameter))
        caps_images.append(pygame.surfarray.array3d(caps_pngs[i]))
        caps_images[i].swapaxes(0,1)    

    # Change the picture name HERE
    main_image = pygame.image.load('Thom.jpg')

    width = n_of_caps_per_row * caps_diameter
    height = int(main_image.get_height() / main_image.get_width() * width)

    main_image = pygame.transform.scale(main_image, (width, height))
    main_image = pygame.surfarray.array3d(main_image).swapaxes(0,1)

    shape = main_image.shape
    radius = caps_diameter / 2

    centers = get_centers(img_width = shape[0], img_height = shape[1], radius = radius)
    indexes = get_images_indexes(image_data = main_image, caps_data = caps_images, centers = centers, radius = radius)

    window = pygame.display.set_mode((main_image.shape[1], main_image.shape[0]))
    clock = pygame.time.Clock()

    for i in range(len(centers)):
        s_x = centers[i][1] - radius
        s_y = centers[i][0] - radius
        e_x = centers[i][1] + radius
        e_y = centers[i][0] + radius

        rect = pygame.Rect(int(s_x), int(s_y), int(e_x - s_x), int(e_y - s_y))

        window.blit(caps_pngs[indexes[i]], rect)

    pygame.display.update()

    new_im = Image.fromarray(pygame.surfarray.array3d(window), 'RGB')
    # Change the final picture name HERE
    new_im.save("Image_caps.png")

    while True:
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                exit()

        clock.tick(20)
        pygame.time.delay(30)


if __name__ == "__main__":
    main()